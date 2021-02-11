#include "server.h"
#include "clientconnection.h"

#include <zypp-core/zyppng/base/EventDispatcher>

#include <iostream>

Server::Server()
{ }

int Server::run(const std::string &sockPath) {

  _loop = zyppng::EventLoop::create();

  pid_t trustedPpid = ::getppid();
  auto  uId = ::getuid();

  auto sockAddr = std::make_shared<zyppng::UnixSockAddr>( sockPath, true );
  // lets open a socket
  _serverSocket = zyppng::Socket::create( AF_UNIX, SOCK_STREAM, 0 );
  if ( !_serverSocket ) {
    perror("socket error");
    return EXIT_FAILURE;
  }

  if ( !_serverSocket->bind( sockAddr ) )  {
    perror("bind error");
    return EXIT_FAILURE;
  }

  if ( !_serverSocket->listen() ) {
    perror("listen error");
    return EXIT_FAILURE;
  }

  _serverSocket->connectFunc( &zyppng::Socket::sigIncomingConnection, [&](){
    auto cSocket = _serverSocket->accept();

    std::cerr << "Incoming connection" << std::endl;

    // check if the connecting process is our parent, otherwise we do not accept the socket.
    // we need to be extra paranoid here because we are running as root so make sure other processes
    // can not connect to our socket and spawn root processes
    struct ucred peerCred;
    socklen_t len = sizeof(struct ucred);
    if ( ::getsockopt( cSocket->nativeSocket(), SOL_SOCKET, SO_PEERCRED, &peerCred, &len) == -1 ) {
      perror("Failed to query SO_PEERCRED");
      cSocket->close();
      return;
    }

    std::cerr << "Our trusted ppid: " << trustedPpid << " and user id " << uId << std::endl;
    std::cerr << "Received connection from pid: " << peerCred.pid << " and user id " << peerCred.uid << std::endl;

    if ( trustedPpid != peerCred.pid ) {
      cSocket->close();
      return;
    }

    // extra paranoid +1 , if we are root and the other process is not also reject the connection
    if ( uId == 0 && peerCred.uid != uId ) {
      cSocket->close();
      return;
    }

    auto client = std::make_shared<ClientConnection>( *this, cSocket );
    _clients.push_back( client );
    zyppng::Base::connectFunc( *client, &ClientConnection::sigClosed, [ this, c = std::weak_ptr(client) ](){
      auto client = c.lock();
      _clients.remove( client );
    }, *client );
  });

  std::cerr << "Zypp-spawn successful, starting loop" << std::endl;
  _loop->run();
  _loop = nullptr;
  return EXIT_SUCCESS;
}

void Server::releasePID(GPid pid) {
  _pids.erase( pid );
}

void Server::trackPID(GPid pid) {
  TrackedPID tracker;
  tracker.pid = pid;

  _loop->eventDispatcher()->trackChildProcess( pid, [this]( int pid, int status ){
    try {
      auto &p = _pids[pid];
      p.exitStatus = status;
      if ( p.callback ) {
        p.callback( pid, status );
        // process exit status delivered, we can remove the data
        _pids.erase(pid);
      }
    }  catch ( const std::out_of_range & ) {
      return;
    }
  });
  _pids.insert( std::make_pair( pid, tracker ) );
}

bool Server::waitPID(GPid pid, bool wait, std::function<void (GPid, gint)> &&fun)  {

  if ( pid < 0 )
    return false;

  try {
    auto &p = _pids[pid];

    // process did exit already, callback invoked right away
    if ( p.exitStatus ) {
      fun( pid, *p.exitStatus );
      _pids.erase(pid);
      return true;
    }

    // just a check, we do not want to wait
    if ( !wait ) {
      fun( 0, 0 );
      return true;
    }

    // process is still running, we need to wait more
    p.callback = std::move(fun);

  } catch ( const std::out_of_range & ) {
    return false;
  }
  return true;
}
