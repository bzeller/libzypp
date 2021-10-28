/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "device.h"
#include <zypp/base/String.h>
#include <zypp/zyppng/media/DeviceHandler>

namespace zyppng {

  Device::Device( const std::string &type, const std::string &name, unsigned int maj, unsigned int min )
    : _type( type )
    , _name( name )
    , _maj_nr ( maj )
    , _min_nr ( min )
  { }

  bool Device::equals(const Device &other)
  {
    if( _type == other._type)
    {
      if( _maj_nr == 0)
        return _name == other._name;
      else
        return _maj_nr == other._maj_nr &&
               _min_nr == other._min_nr;
    }
    return false;
  }

  bool Device::isAttached() const
  {
    return _mountingHandler.size() > 0;
  }

  bool Device::isIdle() const
  {
    if ( !isAttached () )
      return true;

    for ( const auto &hdl: _mountingHandler ) {
      auto ptr = hdl.second.lock ();
      if ( ptr && !ptr->isIdle() )
        return false;
    }

    return true;
  }

  uint Device::addMount( std::shared_ptr<DeviceHandler> hdl )
  {
    const auto first = _nextMountId;
    do {
      _nextMountId++;

      // we did overflow and hit the same ID again,
      // this should never happen but never say never
      if ( _nextMountId == first ) {
        ZYPP_THROW( zypp::Exception("Internal ID overflow") );
      }

    } while ( _mountingHandler.find( _nextMountId ) != _mountingHandler.end()  );
    _mountingHandler.insert( std::make_pair(_nextMountId, hdl ) );
    return _nextMountId;
  }

  void Device::delMount( uint mountId )
  {
    _mountingHandler.erase( mountId );
    if ( _mountingHandler.size() == 0 ) {
      _sigUnmounted.emit();
    }
  }

  std::vector< std::weak_ptr<DeviceHandler> > Device::activeHandlers() const
  {
    std::vector<std::weak_ptr<DeviceHandler>> hdls;
    std::transform( _mountingHandler.begin(), _mountingHandler.end(), std::back_inserter(hdls),
      []( const auto &e ){
        return e.second;
    });
    return hdls;
  }

  const std::unordered_map<std::string, std::any> &Device::properties () const
  {
    return _properties;
  }

  std::unordered_map<std::string, std::any> &Device::properties ()
  {
    return _properties;
  }

  const std::string &Device::type() const
  {
    return _type;
  }

  const std::string &Device::name() const
  {
    return _name;
  }

  unsigned int Device::maj_nr() const
  {
    return _maj_nr;
  }

  unsigned int Device::min_nr() const
  {
    return _min_nr;
  }

  std::string Device::asString() const
  {
    std::string tmp1;
    if( _maj_nr != 0 )
    {
      tmp1 = "[" + zypp::str::numstring( _maj_nr ) + "," +
             zypp::str::numstring( _min_nr ) + "]";
    }
    return _type + "<" + _name + tmp1 + ">";
  }

  SignalProxy<void ()> Device::sigUnmounted()
  {
    return _sigUnmounted;
  }

  SignalProxy<void ()> Device::sigAboutToEject()
  {
    return _sigAboutToEject;
  }

  void Device::prepareEject ()
  {
    _sigAboutToEject.emit();
  }

}
