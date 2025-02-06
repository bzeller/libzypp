/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_APPLICATION_INCLUDED
#define ZYPP_NG_APPLICATION_INCLUDED

#include <zypp-core/zyppng/base/base.h>
#include <zypp-core/zyppng/base/zyppglobal.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( Application );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( EventDispatcher );


  class Application : public Base {

    Application();
    ~Application() override;

    static ApplicationRef create();
    static ApplicationRef instance();


  private:
    EventDispatcherRef _eventDispatcher;
  };

}

#endif //ZYPP_NG_APPLICATION_INCLUDED
