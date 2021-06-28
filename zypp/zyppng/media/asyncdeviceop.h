/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPPNG_MEDIA_ASYNCDEVICEOP_H_INCLUDED
#define ZYPPNG_MEDIA_ASYNCDEVICEOP_H_INCLUDED

#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp/zyppng/media/devicehandler.h>

namespace zyppng {

  class DeviceHandler;

  template<typename Result, typename DeviceHandlerType>
  struct AsyncDeviceOp : public AsyncOp<Result>
  {
    using Ptr = std::shared_ptr<AsyncDeviceOp<Result, DeviceHandlerType>>;
    using ReqId = uint;

    AsyncDeviceOp ( std::shared_ptr<DeviceHandlerType> parent )
      : _parentHandler( std::move(parent) ) {
      _parentHandler->connect( &DeviceHandler::sigShutdown, *this, &AsyncDeviceOp::cancel );
    }

    ~AsyncDeviceOp() { }

    // AsyncOpBase interface
    bool canCancel() override {
      return true;
    }
    void cancel() override {
      if ( !this->isReady() ) {
        handlerShutdown();
        _parentHandler.reset();
      }
    }

  protected:
    // force subclasses to implement this
    virtual void handlerShutdown() = 0;
    std::shared_ptr<DeviceHandlerType> _parentHandler; //!< Our reference to the device handler, we need to keep it alive!
  };

  namespace detail {
    //A async op that is ready right away
    template <typename T, typename DevHdlT>
    struct ReadyDeviceOp : public zyppng::AsyncDeviceOp< T, DevHdlT >
    {
      ReadyDeviceOp( std::shared_ptr<DevHdlT> parent, T &&val ) : AsyncDeviceOp<T, DevHdlT>(parent){
        this->setReady( std::move(val) );
      }

    protected:
      // do nothing, we are already finished
      virtual void handlerShutdown(){}
    };
  }

  template <typename T, typename DevHdlT>
  std::shared_ptr<AsyncDeviceOp<T, DevHdlT>> makeReadyDeviceOp ( std::shared_ptr<DevHdlT> parent, T && result ) {
    return std::make_shared<detail::ReadyDeviceOp<T, DevHdlT>>( parent, std::move(result) );
  }

}



#endif
