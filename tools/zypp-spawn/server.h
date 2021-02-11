#ifndef SERVER_H
#define SERVER_H

#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-core/zyppng/io/Socket>

#include <string>
#include <glib.h>
#include <optional>
#include <functional>
#include <unordered_map>
#include <list>

class ClientConnection;

class Server {

public:
  Server ();

  int run ( const std::string &sockPath );

  void releasePID ( GPid pid );

  // add a PID to the internal list and register a poll on the event loop
  void trackPID ( GPid pid );

  // register a wait PID callback , if the process was ended already this will immediately invoke the callback
  // only one process can wait for a PID
  // if wait is set to true, callback will be invoked with (0,0) to signal that process is still running, no callback
  // is registered
  bool waitPID ( GPid pid, bool wait, std::function<void( GPid, gint )> &&fun );

private:

  struct TrackedPID {
    GPid pid        = -1;
    std::optional<gint> exitStatus;
    std::function<void( GPid, gint )> callback;
  };

  zyppng::EventLoop::Ptr _loop;
  zyppng::Socket::Ptr _serverSocket;
  std::unordered_map<GPid, TrackedPID> _pids;
  std::list< std::shared_ptr<ClientConnection> > _clients;
};

#endif // SERVER_H
