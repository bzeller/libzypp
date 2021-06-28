#include "context.h"
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp/zyppng/media/network/private/mirrorcontrol_p.h>

namespace zyppng {

  Context::Context()
  {
    _zyppEventLoop = EventLoop::create();
  }

  std::shared_ptr<EventLoop> Context::evLoop() const
  {
    return _zyppEventLoop;
  }

  std::shared_ptr<DeviceManager> Context::devMgr () const {
    return _devMgr;
  }

}
