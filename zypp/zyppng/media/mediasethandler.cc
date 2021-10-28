/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "mediasethandler.h"

#include <zypp/zyppng/media/DeviceManager>
#include <zypp/zyppng/media/DeviceHandler>
#include <zypp/zyppng/media/Device>
#include <zypp/zyppng/media/MediaVerifier>
#include <zypp/media/MediaException.h>
#include <zypp/zyppng/Context>

#include <zypp-core/base/Gettext.h>
#include <zypp-core/zyppng/base/private/base_p.h>
#include <zypp-core/zyppng/base/Timer>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/zyppng/pipelines/Wait>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/Date.h>
#include <zypp/base/Regex.h>
#include <zypp/ZConfig.h>
#include <map>
#include <vector>

// One Device can contain multiple medias
// keep devices as long as possible -> conflicts with other MediaHandlers !
// every time we need a media nr we need to check all mounted devices if its the requested device
// -> Map of all medianr->devices
// -> List of all active handlers mounted for a specific medianr
// -> About to eject device signal for forced caching
// -> Should the layer above the mediahandler decide when to switch medias?

namespace zyppng {

  // enable pipeline code
  using namespace zyppng::operators;

  struct DelayedJob {
    virtual ~DelayedJob() {}
    virtual void trigger( DeviceHandlerRef devRef ) = 0;
    virtual void setError ( std::exception_ptr excp ) = 0;
  };

  struct DelayedProvideJob : public AsyncOp<expected<PathRef>>, public DelayedJob
  { };

  class MediaNoFreeDeviceException : public zypp::media::MediaException
  {
  public:
    MediaNoFreeDeviceException( const uint mediaNr ) : MediaException( zypp::str::Format("No free device to handle requests for %1%") % mediaNr) {}
  };

  class MediaSetHandlerPrivate : public BasePrivate {
    public:

      struct MediaDevice {
        enum VerificationState {
          None,     // < the device was not yet probed for the given media nr
          Verified, // < the device was probed and is the desired media
          Failed    // < the device was probed and is NOT the desired media
        };
        Device::Ptr _device;
        std::map<int, VerificationState> _verifiedMediaNrs;
      };

      /*!
       * A DeviceBucket combines a available device for a certain
       * MediaNr, with its handler and a reference count on how many
       * requests currently actively use it. This makes it possible for us
       * to determine which Devices are free for use
       */
      struct DeviceBucket
      {
        DeviceBucket( MediaSetHandlerPrivate &parent, DeviceHandler::Ptr dev ) : _parent(parent) {
        }

        void addRef() {
          _refs++;
        }

        void unRef () {
          _refs--;
        }

        MediaSetHandlerPrivate &_parent;
        DeviceHandler::Ptr _handler;
        Device::Ptr _device;
        int _refs = 0;
        int _currMediaNr = -1; //< The currently mounted mediaNr in this device
      };

      expected<zypp::Pathname> createAttachpoint( const zypp::Url &url_r ) const;
      zypp::Url rewriteUrl ( const zypp::Url & url_r, const uint medianr ) const;
      int findAvailableBucket ( const uint medianr );
      void schedule ();
      void scheduleJobsOn ( const uint medianr, DeviceHandlerRef handler );

      Context  &_zyppContext;
      zypp::Url _baseUrl; // The baseUrl that we use to request devices and to build requests
      std::map<int, std::vector<std::weak_ptr<DelayedJob>>> _pendingRequests; // pending requests

      std::map<int, AsyncOpRef<expected<void>>>
        _runningScheduleJobs; //< Currently running schedule Jobs, consider all medias in the map as locked

      std::unordered_map<int, MediaVerifierRef> _registeredVerifiers; // the verifiers we have registered for each media nr

      std::vector<DeviceBucket> _knownDevices; // this is a list of all devices we currently know of
      std::unordered_map<int, std::vector<int>> _mediaDevices; // map to a list of indices in the _knownDevices list available for a specific medianr
      std::unordered_map<int, DeviceBucket> _activeBuckets; // map of all active buckets for a specific media nr
  };

  expected<zypp::filesystem::Pathname> MediaSetHandlerPrivate::createAttachpoint( const zypp::Url &url_r ) const
  {
    const auto &buildAttachPoint = []( const auto &attach_root ) {
      zypp::Pathname apoint;

      if ( attach_root.empty() || !attach_root.absolute() ) {
        ERR << "Create attach point: invalid attach root: '" << attach_root
            << "'" << std::endl;
        return apoint;
      }

      zypp::PathInfo adir( attach_root );
      if ( !adir.isDir() || ( geteuid() != 0 && !adir.userMayRWX() ) ) {
        DBG << "Create attach point: attach root is not a writable directory: '"
            << attach_root << "'" << std::endl;
        return apoint;
      }

      static bool cleanup_once( true );
      if ( cleanup_once ) {
        cleanup_once = false;
        DBG << "Look for orphaned attach points in " << adir << std::endl;
        std::list<std::string> entries;
        zypp::filesystem::readdir( entries, attach_root, false );
        for ( const std::string &entry : entries )
        {
          if ( !zypp::str::hasPrefix( entry, "AP_0x" ) )
            continue;
          zypp::PathInfo sdir( attach_root + entry );
          if ( sdir.isDir() && sdir.dev() == adir.dev() &&
               ( zypp::Date::now() - sdir.mtime() > zypp::Date::month ) )
          {
            DBG << "Remove orphaned attach point " << sdir << std::endl;
            zypp::filesystem::recursive_rmdir( sdir.path() );
          }
        }
      }

      zypp::filesystem::TmpDir tmpdir( attach_root, "AP_0x" );
      if ( tmpdir ) {
        //@TODO use realpath here
        apoint = tmpdir.path().asString();
        if ( !apoint.empty() ) {
          tmpdir.autoCleanup( false ); // Take responsibility for cleanup.
        } else {
          ERR << "Unable to resolve real path for attach point " << tmpdir
              << std::endl;
        }
      } else {
        ERR << "Unable to create attach point below " << attach_root
            << std::endl;
      }
      return apoint;
    };

    zypp::Pathname aroot;
    zypp::Pathname apoint;
    if ( apoint.empty() ) { // fallback to config value
      aroot = zypp::ZConfig::instance().download_mediaMountdir();
      if ( !aroot.empty() )
        apoint = buildAttachPoint( aroot );
    }

    if ( apoint.empty() ) { // fall back to temp space
      aroot = zypp::filesystem::TmpPath::defaultLocation();
      if ( !aroot.empty() )
        apoint = buildAttachPoint( aroot );
    }

    if ( apoint.empty() ) {
      auto except = zypp::media::MediaBadAttachPointException( url_r );
      except.addHistory( _( "Create attach point: Can't find a writable "
                            "directory to create an attach point" ) );
      return expected<zypp::Pathname>::error( ZYPP_EXCPT_PTR( except ) );
    }
    return expected<zypp::Pathname>::success( apoint );
  }

  zypp::Url MediaSetHandlerPrivate::rewriteUrl(
    const zypp::Url &url_r, const uint medianr ) const
  {
    std::string scheme = url_r.getScheme();
    if ( medianr == 1 || scheme == "cd" || scheme == "dvd" )
      return url_r;

    DBG << "Rewriting url " << url_r << std::endl;

    if( scheme == "iso") {
      std::string isofile = url_r.getQueryParam("iso");
      zypp::str::regex e("^(.*)(cd|dvd|media)[0-9]+\\.iso$", zypp::str::regex::icase);

      zypp::str::smatch what;
      if(zypp::str::regex_match(isofile, what, e)) {
        zypp::Url url( url_r);
        isofile = what[1] + what[2] + zypp::str::numstring(medianr) + ".iso";
        url.setQueryParam("iso", isofile);
        DBG << "Url rewrite result: " << url << std::endl;
        return url;
      }
    } else {
      std::string pathname = url_r.getPathName();
      zypp::str::regex e("^(.*)(cd|dvd|media)[0-9]+(/)?$", zypp::str::regex::icase);
      zypp::str::smatch what;
      if(zypp::str::regex_match(pathname, what, e)) {
        zypp::Url url( url_r);
        pathname = what[1] + what[2] + zypp::str::numstring(medianr) + what[3];
        url.setPathName(pathname);
        DBG << "Url rewrite result: " << url << std::endl;
        return url;
      }
    }
    return url_r;
  }

  int MediaSetHandlerPrivate::findAvailableBucket ( const uint medianr )
  {
    if ( !_mediaDevices.count( medianr ) ) {
      return -1;
#if 0
      const auto &devs = _zyppContext.devMgr()->findDevicesFor( rewriteUrl( _baseUrl, medianr ) );
      if ( devs.empty() ) {
        // no devices found, requests for this medianr need to fail
        return -2;
      }
      for ( const auto &dev : devs ) {
        auto i = std::find_if( _knownDevices.begin(), _knownDevices.end(), [ &dev ]( const auto &dev2 ){
          return dev->equals( *dev2._device );
        });
        if ( i == _knownDevices.end() ) {
          _knownDevices.emplace_back( *this, dev );
          _mediaDevices[medianr].push_back( _knownDevices.size()-1 );
        } else {
          _mediaDevices[medianr].push_back( std::distance( _knownDevices.begin(), i ) );
        }

      }
#endif
    }

    int buck = -1;
    for ( auto i : _mediaDevices[medianr] ) {
      // jackpot, we found a active devicehandler that can serve requests for the medianr we want
      if ( _knownDevices[i]._currMediaNr == medianr ) {
        return i;
      } else if ( _knownDevices[i]._currMediaNr == -1 && !_knownDevices[i]._device->isAttached() ) {
        buck = i;
      }
    }
    return buck;
  }

  void MediaSetHandlerPrivate::schedule ()
  {
    if ( !_pendingRequests.size() )
      return;

    for ( auto &[key,val] : _pendingRequests ) {
      // first step, we try to find a bucket that can immediately handle the requests
      int availBucket = findAvailableBucket( key );
      if ( availBucket >= 0 )
      {
        // schedule requests
        continue;
      }

      // check if we are already scheduling jobs for that media nr
      if ( _runningScheduleJobs.count (key) )
        continue;

      // when we reach here, we have no existing handler that is known to handle the requested medianr
      // we need to check
      const auto &baseUrl = rewriteUrl( _baseUrl, key );
      const auto &devs = _zyppContext.devMgr()->findDevicesFor( baseUrl );
      if ( devs.empty() )
      {
        // no devices found, requests for this medianr need to fail
        continue;
      }

      // okay , we found possible devices, lets check if they are what we need
      auto aps = devs | transform( [ &, this, key = key ]( const auto &dev ){
        return createAttachpoint( baseUrl )
               | mbind( [ &, this ]( zypp::Pathname &&ap ) {
                   return _zyppContext.devMgr()->attachTo( dev, baseUrl, ap );
                 })
               | mbind( [ this, key ]( DeviceHandlerRef &&handler ) {
                   // IF we reach here we have a handler, now we need to verify the media
                   MediaVerifierRef ver;
                   if ( auto v = _registeredVerifiers.find ( key ); v!=_registeredVerifiers.end() ) {
                     ver = v->second;
                   } else {
                     const auto &newEl = _registeredVerifiers.insert( { key, zyppng::NoVerifier::create() } );
                     ver = newEl.first->second;
                   }

                   return ver->isDesiredMedia ( *handler )
                            | [ handler ]( expected<void> &&res ) -> expected<DeviceHandlerRef> {
                     if ( res.success () ) {
                       return  expected<DeviceHandlerRef>::success(handler);
                     }
                     return  expected<DeviceHandlerRef>::error( ZYPP_EXCPT_PTR ( zypp::media::MediaException("Could not verify the media.") ));;
                   };
                 });
      }) | waitFor<expected<DeviceHandlerRef>>()
         | [this, key = key]( std::vector<expected<DeviceHandlerRef>> &&results ) -> expected<void> {
           for ( const auto &res : results ) {
             if ( !res )
               continue;
             // we found a handler that verified, use the first one and schedule jobs on it
             scheduleJobsOn ( key, res.get () );
             return expected<void>::success ();
           }
           // found nothing, either none of the devices contains the media we are looking for or contains NO media at all
           MIL << "Could not find a usable handler for media nr " << key << " waiting for more ressources" << std::endl;
           return expected<void>::error( ZYPP_EXCPT_PTR (MediaNoFreeDeviceException(key)));
         };

      // we cannot remove the op via the pipeline, this would crash since the callback would destroy the last reference to the op while
      // its still executed
      if ( aps->isReady () ) {

      } else {
        aps->connectFunc( &AsyncOp<expected<void>>::sigReady, [this, key=key](){
            _runningScheduleJobs.erase (key);
        }, this );
        _runningScheduleJobs[key] = std::move(aps);
      }
    }
  }

  void MediaSetHandlerPrivate::scheduleJobsOn(const uint medianr, DeviceHandlerRef handler)
  {

  }

  AsyncOpRef<expected<PathRef> > MediaSetHandler::provideFile( const zypp::OnMediaLocation &file )
  {

  }




  ZYPP_IMPL_PRIVATE(MediaSetHandler)
}
