/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/

#include "private/pathref_p.h"

#include <zypp/zyppng/media/DeviceHandler>

namespace zyppng {

  PathRefPrivate::PathRefPrivate()
  {}

  PathRefPrivate::PathRefPrivate( std::shared_ptr<DeviceHandler> &&ref, zypp::Pathname path )
    : _handler( std::move(ref) )
    , _path( std::move(path) )
  {
    // this PathRef points directly into the devices mountpoint, we do never delete files there
    _path.resetDispose();
    init();
  }

  PathRefPrivate::PathRefPrivate( zypp::Pathname path )
    : _handler( nullptr )
    , _path( std::move(path) )
  {
    init();
  }

  PathRefPrivate::~PathRefPrivate()
  {
    if ( _handlerConnection )
      _handlerConnection.disconnect();
    _released.emit();
  }

  void PathRefPrivate::init()
  {
    if ( !_handler )
      return;
    _handlerConnection = _handler->sigShutdown().connect( sigc::mem_fun( *this, &PathRefPrivate::handlerShutdown ) );
  }

  void PathRefPrivate::handlerShutdown()
  {
    // our path turns invalid now
    reset();
  }

  void PathRefPrivate::reset()
  {
    _handlerConnection.disconnect();
    _handler.reset();
    _path = zypp::ManagedFile();
  }

  PathRef::PathRef() : d_ptr( std::make_shared<PathRefPrivate>() )
  { }

  PathRef::PathRef( std::shared_ptr<DeviceHandler> &&ref, zypp::filesystem::Pathname path )
  { }

#if 0
  PathRef::PathRef( zypp::filesystem::Pathname path )
  {
  }
#endif

  PathRef::~PathRef()
  { }

  const std::shared_ptr<DeviceHandler> &PathRef::handler() const
  {
    return d_ptr->_handler;
  }

  SignalProxy<void()> PathRef::sigReleased ()
  {
    return d_ptr->_released;
  }

  const zypp::Pathname &PathRef::path() const
  {
    return d_ptr->_path;
  }

}