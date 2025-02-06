/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "context.h"
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-media/ng/Provide>

namespace zyppng {

  ZYPP_IMPL_PRIVATE_CONSTR( Context )
    : UserInterface( )
  {
    _provider = Provide::create( _providerDir );

    // @TODO should start be implicit as soon as something is enqueued?
    _provider->start();
  }

  ProvideRef Context::provider() const
  {
    return _provider;
  }

  KeyRingRef Context::keyRing() const
  {
    return d_func()->_zyppPtr->keyRing();
  }

  zypp::ZConfig &Context::config()
  {
    return zypp::ZConfig::instance();
  }

  zypp::ResPool Context::pool()
  {
    return zypp::ResPool::instance();
  }

  zypp::ResPoolProxy Context::poolProxy()
  {
    return zypp::ResPool::instance().proxy();
  }

  zypp::sat::Pool Context::satPool()
  {
    return zypp::sat::Pool::instance();
  }
}
