/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/

#include "devicemanager.h"
#include "device.h"
#include "devicehandler.h"

#include <zypp/media/MediaException.h>
#include <zypp-core/zyppng/base/private/base_p.h>
#include <zypp/zyppng/media/handler/cdromhandler_p.h>
#include <zypp/zyppng/media/handler/networkhandler_p.h>

#include <vector>

#include <zypp/base/LogControl.h>
#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::DeviceManager"

namespace zyppng {

  DeviceType::~DeviceType()
  { }

  bool DeviceType::isVirtual() const
  {
    return _isVirtual;
  }

  const std::string &DeviceType::type() const
  {
    return _type;
  }

  DeviceManager & DeviceType::deviceManager()
  {
    return _devMgr;
  }

  const DeviceManager & DeviceType::deviceManager() const
  {
    return _devMgr;
  }

  DeviceType::DeviceType( DeviceManager &devMgr, const std::string &typeName, bool isVirtual )
    : _type( typeName )
    , _isVirtual( isVirtual )
    , _devMgr ( devMgr )
  {
  }

  expected<void> DeviceType::prepareEject ( Device &dev )
  {
    if ( dev.isAttached() ) {
      dev.prepareEject();
      if ( dev.isAttached() ) {
        return expected<void>::error( ZYPP_EXCPT_PTR( zypp::media::MediaException("Failed to release ressources of device")) );
      }
    }
    return expected<void>::success();
  }

  void DeviceType::umountHelper( const zypp::Pathname &mountpoint )
  {
    DBG << "Cleaning up mountpoint: " << mountpoint << std::endl;
    try {
      zypp::media::Mount umount;
      umount.umount( mountpoint.asString() );
    } catch( const zypp::Exception& e ) {
      ZYPP_CAUGHT(e);
      ERR << "Failed to umount" << mountpoint << " with error (" << e.asString() << ")" << std::endl;
    } catch ( ... ) {
      ERR << "Failed to umount" << mountpoint << " with error (unknown exception)" << std::endl;
    }
  }

  class DeviceManagerPrivate : public BasePrivate
  {
  public:
    DeviceManagerPrivate( DeviceManager &parent ) : BasePrivate(parent) {}
    std::unordered_map< std::string, std::shared_ptr<DeviceType> >  _knownTypes;
  };

  DeviceManager::DeviceManager() : Base( *( new DeviceManagerPrivate( *this ) ) )
  {
    this->registerDeviceType( std::make_shared<CDRomDeviceType>(*this) );
    this->registerDeviceType( std::make_shared<NetworkDeviceType>(*this) );
  }

  std::shared_ptr<DeviceManager> DeviceManager::create()
  {
    return std::shared_ptr<DeviceManager>( new DeviceManager() );
  }

  std::vector< Device::Ptr > DeviceManager::findDevicesFor( const zypp::Url &url , const std::vector<std::string> &filters )
  {
    Z_D();
    if ( !url.isValid() ) {
      ERR << "Invalid URL passed to DeviceManager:" << url << std::endl;
      return {};
    }

    for ( auto &type :d->_knownTypes ) {
      if ( type.second->canHandle( url ) ) {

        MIL << "Found devicetype " << type.second->type() << " for URL " << url << std::endl;

        auto dev = type.second->detectDevices( url, filters );
        if ( dev.size() > 0 ) {
          MIL << "Detected " << dev.size() << " devices for URL " << url << std::endl;
          return dev;
        }
      }
    }

    MIL << "No device type can handle the URL " << url << " aborting." << std::endl;
    return {};
  }

  expected<std::shared_ptr<DeviceHandler>>  DeviceManager::attachTo( Device::Ptr dev, const zypp::Url &baseurl, const zypp::filesystem::Pathname &preferredAttachPoint )
  {
    Z_D();
    if ( auto t = d->_knownTypes.find( dev->type() ); t!= d->_knownTypes.end() ) {

      if ( !t->second->canHandle( baseurl ) )
        return expected<std::shared_ptr<DeviceHandler>>::error( std::make_exception_ptr( zypp::media::MediaException("Device type and URL do not match")) );

      return t->second->attachDevice( dev, baseurl, preferredAttachPoint );
    } else {
      return expected<std::shared_ptr<DeviceHandler>>::error( std::make_exception_ptr( zypp::media::MediaException("Bad device")) );
    }
  }

  void DeviceManager::registerDeviceType( std::shared_ptr<DeviceType> type )
  {
    Z_D();
    if ( d->_knownTypes.count( type->type() ) ) {
      ERR << "Trying to re-register device type " << type->type() << " is not supported, ignoring!" << std::endl;
      return;
    }
    d->_knownTypes.insert( std::make_pair( type->type(), type ) );
  }

  std::shared_ptr<DeviceType> DeviceManager::deviceType(const std::string &devType)
  {
    Z_D();
    if ( d->_knownTypes.count( devType ) ) {
      return d->_knownTypes[devType];
    }
    return nullptr;
  }

  bool DeviceManager::isUseableAttachPoint( const zypp::Pathname &path ) const
  {
    Z_D();
    const std::string our( path.asString() );

    // checks if the given path \a our intersects with the mountpoint in \a mnt
    const auto &doesIntersect = []( const std::string &our, const std::string &mnt ) {
      if( our == mnt) {
        // already used as attach point
        return true;
      } else if( mnt.size() > our.size()   &&
                mnt.at(our.size()) == '/' &&
                !mnt.compare(0, our.size(), our) ) {
        // mountpoint is below our path
        // (would hide the content)
        return true;
      }
      return false;
    };

    // check against existing device handlers
    for ( const auto &type : d->_knownTypes ) {
      for ( const auto &handler : type.second->activeHandlers() ) {
        auto ptr = handler.lock();
        if ( !ptr )
          continue;

        if ( doesIntersect(our, ptr->attachPoint()->asString()) )
          return false;
      }
    }

    // check against system mount entries
    const auto &entries( zypp::media::Mount::getEntries() );
    for( const auto &e : entries ) {
      if ( doesIntersect(our, zypp::Pathname(e.dir).asString()) )
        return false;
    }
    return true;
  }

  /**
   * @TODO cache mtab at least per DeviceManager
   */
  expected<zypp::media::MountEntry> DeviceManager::findMount( const zypp::Pathname &path )
  {
    const auto &allEntries = zypp::media::Mount::getEntries();
    for ( const auto &entry: allEntries ) {
      if ( zypp::Pathname( entry.dir) == path )
        return expected<zypp::media::MountEntry>::success( entry );
    }
    return expected<zypp::media::MountEntry>::error( ZYPP_EXCPT_PTR( zypp::media::MediaException("Unable to find the mountpoint in mtab") ) );
  }

  std::vector<zypp::media::MountEntry> DeviceManager::findMounts( const Device &dev )
  {
    zypp::PathInfo devNode( dev.name() );

    const auto &allEntries = zypp::media::Mount::getEntries();
    for ( const auto &entry: allEntries ) {

    }
  }

  ZYPP_IMPL_PRIVATE( DeviceManager )
}

