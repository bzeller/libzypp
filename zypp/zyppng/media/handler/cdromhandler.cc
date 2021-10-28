/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "cdromhandler_p.h"
#include <zypp/zyppng/media/Device>
#include <zypp/media/Mount.h>
#include <zypp/media/MediaException.h>
#include <any>

#include <zypp/base/LogControl.h>
#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::CDRomDeviceType"

extern "C"
{
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#if HAVE_UDEV
#include <libudev.h>
#endif
}

/*
** If defined to the full path of the eject utility,
** it will be used additionally to the eject-ioctl.
*/
#define EJECT_TOOL_PATH "/usr/bin/eject"

namespace zyppng {

  CDRomDeviceType::CDRomDeviceType( DeviceManager &devMgr ) : DeviceType( devMgr, "cdrom" )
  { }

  std::vector<std::shared_ptr<Device>> CDRomDeviceType::detectDevices( const zypp::Url &url, const std::vector<std::string> &filters )
  {
    // helper lambda to add a new device to the internal registry, returning it if its either created or already known
    const auto registerDevice = [ this ]( const zypp::PathInfo &devnode, std::string_view reason ) -> std::shared_ptr<Device> {
        if ( devnode.isBlk() ) {
          const auto &devKey = std::make_pair( devnode.devMajor(), devnode.devMinor() );
          if ( _sysDevs.count(devKey) ) {
            // we already know the device
            return _sysDevs.at(devKey);
          }
          auto media = std::make_shared<Device>( "cdrom", devnode.path().asString(), devnode.devMajor(), devnode.devMinor() );
          DBG << "Found (" << reason << "): " << *media << std::endl;
          _sysDevs[ devKey ] = media;
          return media;
        }
        return nullptr;
    };

    if ( url.getScheme() != "dvd" && url.getScheme() != "cd" ) {
      ERR << "Unsupported schema in the Url: " << url.asString() << std::endl;
      return {};
    }

    // detect devices available in the system if we did not do it before:
    if ( !_detectedSysDevs ) {
      _detectedSysDevs = true;

  #ifdef HAVE_UDEV
      // http://www.kernel.org/pub/linux/utils/kernel/hotplug/libudev/index.html
      zypp::AutoDispose<struct udev *> udev( ::udev_new(), ::udev_unref );
      if ( ! udev ) {
        ERR << "Can't create udev context." << std::endl;
      } else {
        zypp::AutoDispose<struct udev_enumerate *> enumerate( ::udev_enumerate_new(udev), ::udev_enumerate_unref );
        if ( ! enumerate ) {
          ERR << "Can't create udev list entry." << std::endl;
        } else {
          ::udev_enumerate_add_match_subsystem( enumerate, "block" );
          ::udev_enumerate_add_match_property( enumerate, "ID_CDROM", "1" );
          ::udev_enumerate_scan_devices( enumerate );

          struct udev_list_entry * entry = 0;
          udev_list_entry_foreach( entry, ::udev_enumerate_get_list_entry( enumerate ) )
          {
            zypp::AutoDispose<struct udev_device *> device( ::udev_device_new_from_syspath( ::udev_enumerate_get_udev( enumerate ),
                                                              ::udev_list_entry_get_name( entry ) ),
              ::udev_device_unref );
            if ( ! device )
            {
              ERR << "Can't create udev device." << std::endl;
              continue;
            }

            const char * devnodePtr( ::udev_device_get_devnode( device ) );
            if ( ! devnodePtr ) {
              ERR << "Got NULL devicenode." << std::endl;
              continue;
            }

            // In case we need it someday:
            //const char * mountpath = ::udev_device_get_property_value( device, "FSTAB_DIR" );
            auto dev = registerDevice( zypp::PathInfo ( devnodePtr ), "udev" );
            if ( dev ) {
              if ( ::udev_device_get_property_value( device, "ID_CDROM_DVD" ) ) {
                dev->properties()["DVD"] = true;
              } else {
                dev->properties()["DVD"] = false;
              }
            }
          }
        }
    #endif
        if ( _sysDevs.empty() )
        {
          WAR << "CD/DVD drive detection with UDEV failed! Guessing..." << std::endl;
          auto dev = registerDevice( zypp::PathInfo ( "/dev/dvd" ), "GUESS" );
          if ( dev && dev->properties().count("DVD") == 0 )
                dev->properties()["DVD"] = true;
          dev = registerDevice( zypp::PathInfo ( "/dev/cdrom" ), "GUESS" );
          if ( dev && dev->properties().count("DVD") == 0 )
                dev->properties()["DVD"] = false;
        }
      }
    }

    std::vector< std::shared_ptr<Device> > result;

    // now we need to filter the devices based on the URL ,
    // the URL might even contain a device we did not even detect
    // as usual, its mayhem
    std::string devices = url.getQueryParam( "devices" );
    if ( ! devices.empty() ) {
      std::vector<std::string> words;
      zypp::str::split( devices, std::back_inserter(words), "," );
      for ( const std::string & device : words )
      {
        if ( device.empty() )
          continue;

        auto devPtr = registerDevice( zypp::PathInfo(device), "url" );
        if ( !devPtr ) {
          WAR << "Skipping device " << device << " from URL, not a valid device." << std::endl;
          continue;
        }
        result.push_back( devPtr );
      }
    }
    else {
      result.reserve( _sysDevs.size() );
      for ( const auto &i : _sysDevs ) {
        result.push_back( i.second );
      }
    }

    return result;
  }

  expected< std::shared_ptr<DeviceHandler> > CDRomDeviceType::attachDevice( std::shared_ptr<Device> dev, const zypp::Url &baseurl, const zypp::filesystem::Pathname &attachPoint )
  {
    using Ret = expected< std::shared_ptr<DeviceHandler> >;

    // Linux supports mounting a device multiple times since 2.4.
    // So as long as the attachpoint is usable we go for it.
    if ( !deviceManager().isUseableAttachPoint( attachPoint ) ) {
      return Ret::error( ZYPP_EXCPT_PTR( zypp::media::MediaBadAttachPointException( baseurl) ) );
    }

    std::string options = baseurl.getQueryParam( "mountoptions" );
    if ( options.empty() ) {
      options="ro";
    }

    std::list<std::string> filesystems;

    filesystems.push_back("iso9660");

    // if DVD, try UDF filesystem after iso9660
    if ( baseurl.getScheme() == "dvd" )
      filesystems.push_back("udf");

    std::exception_ptr err;
    zypp::media::Mount mount;
    for ( const auto &fs : filesystems ) {
      try {
        err = nullptr;
        mount.mount( dev->name(), attachPoint.asString(), fs, options );

        DBG << "Mounting successful! YAY" << std::endl;

        // todo , wait for mount to become ready!
        DeviceAttachPointRef mountP( attachPoint, &DeviceType::umountHelper );

        DBG << "Make the DeviceAPRef" << std::endl;

        auto hdl = std::shared_ptr<CDRomDeviceHandler>( new CDRomDeviceHandler(*dev, baseurl, deviceManager() ) );
        hdl->setAttachPoint( std::move(mountP) );

        DBG << "Returning handler NOW" << std::endl;
        return Ret::success( hdl );

      } catch ( const zypp::media::MediaMountException &e ) {
        ZYPP_CAUGHT(e);
        if( !e.mountOutput().empty()) {
          err = ZYPP_EXCPT_PTR ( zypp::media::MediaMountException(e.mountError(),
                  baseurl.asString(),
                  attachPoint.asString(),
                  e.mountOutput()));
        } else {
          err = ZYPP_EXCPT_PTR ( zypp::media::MediaMountException("Mounting media failed",
                                 baseurl.asString(),
                                 attachPoint.asString()) );
        }
      } catch (const zypp::Exception &e) {
        err = std::current_exception();
        ZYPP_CAUGHT(e);
      } catch ( ... ) {
        err = std::current_exception();
      }
    }

    if ( err )
      return Ret::error( err );

    return Ret::error( ZYPP_EXCPT_PTR(zypp::media::MediaMountException("Mounting media failed", baseurl.asString(), attachPoint.asString())) );
  }

  expected<void> CDRomDeviceType::ejectDevice( Device &dev )
  {
    const auto &ex = prepareEject( dev );
    if (!ex)
      return ex;

    int fd = ::open( dev.name().c_str(), O_RDONLY|O_NONBLOCK|O_CLOEXEC );
    int res = -1;

    if ( fd != -1)  {
      res = ::ioctl( fd, CDROMEJECT );
      ::close( fd );
    }

    if ( res ) {
      if( fd == -1) {
        WAR << "Unable to open '" << dev
            << "' (" << ::strerror( errno ) << ")" << std::endl;
      } else {
        WAR << "Eject " << dev
            << " failed (" << ::strerror( errno ) << ")" << std::endl;
      }

#if defined(EJECT_TOOL_PATH)
      DBG << "Try to eject " << dev << " using "
        << EJECT_TOOL_PATH << " utility" << std::endl;

      const char *cmd[3];
      cmd[0] = EJECT_TOOL_PATH;
      cmd[1] = dev.name().c_str();
      cmd[2] = NULL;
      zypp::ExternalProgram eject( cmd, zypp::ExternalProgram::Stderr_To_Stdout );

      for(std::string out( eject.receiveLine());
          out.length(); out = eject.receiveLine()) {
        DBG << " " << out;
      }
      DBG << std::endl;

      if ( eject.close() != 0 ) {
        WAR << "Eject of " << dev << " failed." << std::endl;
        return expected<void>::error( ZYPP_EXCPT_PTR(zypp::media::MediaNotEjectedException(dev.name())) );
      }
#else
      return expected<void>::error( ZYPP_EXCPT_PTR(zypp::media::MediaNotEjectedException(dev.name())) );
#endif

    }
    MIL << "Eject of " << dev << " successful." << std::endl;
    return expected<void>::success();
  }

  bool CDRomDeviceType::canHandle( const zypp::Url &url )
  {
    return ( url.getScheme() == "cd" || url.getScheme() == "dvd" );
  }

  std::vector< std::weak_ptr<DeviceHandler> > CDRomDeviceType::activeHandlers()
  {
    std::vector<std::weak_ptr<DeviceHandler>> res;
    std::for_each( _sysDevs.begin(), _sysDevs.end(), [ &res ]( const auto &dev ){
      const auto &devHandlers = dev.second->activeHandlers();
      res.insert( res.end(), devHandlers.begin(), devHandlers.end() );
    });
    return res;
  }

  CDRomDeviceHandler::CDRomDeviceHandler( Device &dev, const zypp::Url &baseUrl, DeviceManager &parent )
    : DeviceHandler( dev, baseUrl, baseUrl.getPathName(), parent )
  {
    setDoesLocalMirror( false );
  }

  bool zyppng::CDRomDeviceHandler::isIdle() const
  {
    // we are always idle because we can immediately fullfill all requests
    return true;
  }


}
