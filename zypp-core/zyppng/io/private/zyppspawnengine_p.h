#ifndef ZYPPNG_IO_PRIVATE_ZYPPSPAWNENGINE_H
#define ZYPPNG_IO_PRIVATE_ZYPPSPAWNENGINE_H

#include "abstractspawnengine_p.h"
#include <sys/socket.h>
#include <cstring>
#include <zypp/AutoDispose.h>

namespace zyppng {
  class ZyppSpawnEngine : public AbstractSpawnEngine
  {
  public:

    constexpr static std::string_view sockNameTemplate = "/org/zypp/zypp-spawn/%1";
    using HeaderSizeType = uint32_t;

    virtual ~ZyppSpawnEngine();
    bool start(const char * const *argv, int stdin_fd, int stdout_fd, int stderr_fd) override;
    bool isRunning(bool wait) override;

  private:
    bool ensureConnection ();
    zypp::AutoFD _sock = -1;
  };
}



#endif //ZYPPNG_IO_PRIVATE_ZYPPSPAWNENGINE_H
