#include "application.h"
#include <zypp-core/zyppng/base/eventdispatcher.h>
#include <zypp-core/zyppng/base/private/threaddata_p.h>


namespace zyppng {

  namespace {
    ApplicationWeakRef& applInstance( ) {
        static ApplicationWeakRef ref;
        return ref;
    }
  }

  Application::Application()
    : _eventDispatcher( ThreadData::current().ensureDispatcher() )
  {
    assert( applInstance().expired() );
  }

  Application::~Application()
  { }

  ApplicationRef Application::create()
  {
    auto inst = std::make_shared<Application>();
    applInstance() = inst;
    return inst;
  }

  ApplicationRef Application::instance()
  {
    return applInstance().lock();
  }

}
