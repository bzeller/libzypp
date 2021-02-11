#include "clientconnection.h"
#include "server.h"

#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>

#include <sys/prctl.h> // prctl(), PR_SET_PDEATHSIG

ClientConnection::ClientConnection(Server &srv, zyppng::Socket::Ptr sock) : _server( &srv ) {
  _socket = sock->releaseSocket();
  _notify = zyppng::SocketNotifier::create( _socket,  zyppng::SocketNotifier::Read | zyppng::SocketNotifier::Error );
  connect( *_notify, &zyppng::SocketNotifier::sigActivated, *this, &ClientConnection::onSocketActivated );
}

ClientConnection::~ClientConnection() {
  if ( _socket != -1 )
    ::close( _socket );
  if ( _receivedFDs.size() ) {
    cleanupUnusedFds();
  }
}

zyppng::SignalProxy<void ()> ClientConnection::sigClosed() {
  return _sigClosed;
}

void ClientConnection::onSocketActivated(const zyppng::SocketNotifier &, int event) {
  switch (event) {
    case zyppng::SocketNotifier::Read:
      onBytesAvailable();
      break;
    case zyppng::SocketNotifier::Error:
      onSocketError();
    case zyppng::SocketNotifier::Write:
      onReadyWrite();
  }
}

void ClientConnection::onBytesAvailable() {

  if ( _state == WaitForHeader ) {

    if ( _readBuffer.size() < sizeof(HeaderSizeType) ) {
      const auto read = readBytesFromSocket( false, sizeof(HeaderSizeType) - _readBuffer.size() );
      if ( read == 0 ) {
        // remote close
        std::cerr << "Client disconnected, cleaning up ressources" << std::endl;
        closeSocket();
      }
    }

    // check if we need to wait more
    if ( _readBuffer.size() < sizeof(HeaderSizeType) )
      return;

    HeaderSizeType size = 0;
    _readBuffer.read( (char *)&size, sizeof(HeaderSizeType) );
    _pendingMessageSize = size;
    _state = ReceivingRequest;

  } else if ( _state == ReceivingRequest ) {

    if ( !_pendingMessageSize ) {
      std::cerr << "Invalid state, there should be a pendingMessageSize." << std::endl;
      closeSocket();
      return;
    }

    if ( (int32_t)_readBuffer.size() < *_pendingMessageSize ) {
      const auto read = readBytesFromSocket( true, *_pendingMessageSize - _readBuffer.size() );
      if ( read == 0 ) {
        // remote close
        std::cerr << "Client disconnected, cleaning up ressources" << std::endl;
        closeSocket();
        return;
      }
    }

    // check if we need to wait more
    if ( (int32_t)_readBuffer.size() < *_pendingMessageSize )
      return;

    // we have all data from the request, disable read notification
    _notify->setMode( zyppng::SocketNotifier::Error );
    _state = ProcessingRequest;

    zyppng::ByteArray message( *_pendingMessageSize, '\0' );
    _readBuffer.read( message.data(), *_pendingMessageSize );
    _pendingMessageSize.reset();

    zypp::proto::Envelope m;
    if (! m.ParseFromArray( message.data(), message.size() ) ) {
      //broken message
      std::cout << "Malformed message on stream." << std::endl;
      zypp::spawn::proto::RequestFailed res;
      res.set_error( "Malformed message on stream." );
      _state = ErrorState;
      sendMessage ( res );
      return;
    }

    handleMessage(m);
  }
}

void ClientConnection::cleanupUnusedFds() {
  // close all fds we did not use
  for ( const auto fd : _receivedFDs ) {
    if ( fd != -1 ) {
      ERR << "Closing unused FD, this should not happen." << std::endl;
      ::close(fd);
    }
  }
  _receivedFDs.clear();
}

void ClientConnection::handleMessage( const zypp::proto::Envelope &m )
{
  const auto &mName = m.messagetypename();
  if (  mName == "zypp.spawn.proto.SpawnProcess" )  {

    zypp::spawn::proto::SpawnProcess spawnReq;
    if ( !spawnReq.ParseFromString( m.value() ) ) {
      handleMalformedMessage();
      return;
    }

    if ( spawnReq.fdintrailer_size() != (int)_receivedFDs.size() ) {
      std::cerr << "Did not receive the expected fds " << spawnReq.fdintrailer_size()<< "!=" << _receivedFDs.size() << std::endl;
      zypp::spawn::proto::RequestFailed res;
      res.set_error( "Did not receive the expected fds" );
      sendMessage ( res );
      return;
    }

    //fds in the order we pass it to g_spawn in, out, err
    int fds[]{ -1, -1, -1 };
    for ( int i = 0; i < spawnReq.fdintrailer_size(); i++ ) {
      fds[ spawnReq.fdintrailer(i) ] = _receivedFDs.at(i);
      // mark a received fd as used, so it is not closed by the cleanup
      _receivedFDs.at(i) = -1;
    }

    // build the argv
    std::vector<gchar *> argvPtrs ( spawnReq.argv_size() + 1 , nullptr );
    for ( int i = 0; i < spawnReq.argv_size(); i++ ) {
      argvPtrs[i] = spawnReq.mutable_argv(i)->data();
    }

    // build the env var ptrs
    std::vector<gchar *> envPtrs ( spawnReq.env_size() + 1 , nullptr );
    for ( int i = 0; i < spawnReq.env_size(); i++ ) {
      envPtrs[i] = spawnReq.mutable_env(i)->data();
    }

    struct ForkData {
      int pidParent = -1;
      zypp::spawn::proto::SpawnProcess *p;
    } data {
      ::getpid(),
          &spawnReq
    };

    std::cerr << "Going to chroot? " << spawnReq.chroot() << std::endl;

    GPid childPid = -1;
    g_autoptr(GError) error = NULL;
    g_spawn_async_with_fds(
        nullptr,
        argvPtrs.data(),
        envPtrs.data(),
        GSpawnFlags(G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH_FROM_ENVP),
        []( gpointer data ){
        ForkData *d = reinterpret_cast<ForkData *>(data);

        bool doChroot = !d->p->chroot().empty();
        std::string chdir = d->p->chdir();
        std::string execError;

        if ( doChroot ) {
          if ( chroot( d->p->chroot().data() ) == -1 ) {
            execError = zypp::str::form( "Can't chroot to '%s' (%s).", d->p->chroot().data(), strerror(errno) );
            std::cerr << execError << std::endl;// After fork log on stderr too
            _exit (128); // No sense in returning! I am forked away!!
          }

          if ( chdir.empty() ) {
            chdir = "/";
          }
        }

        if ( !chdir.empty() && ::chdir( chdir.data() ) == -1 )
        {
          execError = doChroot ? zypp::str::form( "Can't chdir to '%s' inside chroot '%s' (%s).", chdir.data(), d->p->chroot().data(), strerror(errno) )
                               : zypp::str::form( "Can't chdir to '%s' (%s).", chdir.data(), strerror(errno) );
          std::cerr << execError << std::endl; // After fork log on stderr too
          _exit (128);			     // No sense in returning! I am forked away!!
        }

        if ( d->p->diewithparent() ) {
          // process dies with us
          int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
          if (r == -1) {
            //ignore if it did not work, worst case the process lives on after the parent dies
            std::cerr << "Failed to set PR_SET_PDEATHSIG" << std::endl;// After fork log on stderr too
          }

          // test in case the original parent exited just
          // before the prctl() call
          pid_t ppidNow = getppid();
          if (ppidNow != d->pidParent ) {
            std::cerr << "PPID changed from "<<d->pidParent<<" to "<< ppidNow << std::endl;// After fork log on stderr too
            _exit(128);
          }
        }
      },
      &data,
      &childPid,
      fds[0],  //in
      fds[1],  //out
      fds[2],  //err
      &error
    );

    // make sure to close the FDs on our side
    ::close( fds[0] );
    ::close( fds[1] );
    ::close( fds[2] );

    if ( !error ) {
      _server->trackPID( childPid );

      zypp::spawn::proto::SpawnSuccess res;
      res.set_pid( childPid );
      sendMessage( res );
    } else {
      zypp::spawn::proto::RequestFailed res;
      res.set_error( (char *)error->message );
      sendMessage( res );
    }

  } else if ( mName == "zypp.spawn.proto.WaitPid" ) {

    zypp::spawn::proto::WaitPid waitReq;
    if ( !waitReq.ParseFromString( m.value() ) ) {
      handleMalformedMessage();
      return;
    }

    bool success = _server->waitPID( waitReq.pid(), waitReq.wait(), [this]( GPid pid, int status ) {
        zypp::spawn::proto::WaitPidResult res;
        res.set_ret( pid );
        res.set_status( status );
        sendMessage(res);
    });

    if (!success) {
      zypp::spawn::proto::RequestFailed res;
      res.set_error("Could not register to wait for the requested PID");
      sendMessage(res);
    }

  } else if ( mName == "zypp.spawn.proto.ReleaseProcess" ) {

    zypp::spawn::proto::ReleaseProcess releaseReq;
    if ( !releaseReq.ParseFromString( m.value() ) ) {
      handleMalformedMessage();
      return;
    }

    _server->releasePID( releaseReq.pid() );
    prepareForNextRequest();

  } else {
    zypp::spawn::proto::RequestFailed res;
    _state = ErrorState;
    res.set_error( "Invalid message received." );
    sendMessage( res );
  }
}

void ClientConnection::prepareForNextRequest()
{
  _state = WaitForHeader;
  _pendingMessageSize.reset();
  _writeBuffer.clear();
  _readBuffer.clear();
  cleanupUnusedFds();
  _notify->setMode( zyppng::SocketNotifier::Read | zyppng::SocketNotifier::Error );
}

void ClientConnection::handleMalformedMessage()
{
  std::cerr << "Malformed message on stream." << std::endl;
  zypp::spawn::proto::RequestFailed res;
  res.set_error( "Malformed message on stream." );
  _state = ErrorState;
  sendMessage ( res );
}

size_t ClientConnection::readBytesFromSocket( bool expectAncillary , size_t bytesToRead )
{
  int bytes = zyppng::SocketPrivate::bytesAvailableOnSocket( _socket );
  if ( bytes == 0 ) {
    // ioctl might be wrong try to read something
    bytes = bytesToRead > 0 ? bytesToRead : 4096;
  }

  if ( bytesToRead > 0 && (size_t)bytes > bytesToRead )
    bytes = bytesToRead;

  struct iovec iov;
  iov.iov_base = _readBuffer.reserve( bytes );
  iov.iov_len  = bytes;

  struct msghdr msgh;
  msgh.msg_name = nullptr;
  msgh.msg_namelen = 0;
  msgh.msg_iov = &iov;
  msgh.msg_iovlen = 1;

  // according to unix(7) man page which states:
  // When sending ancillary data with sendmsg(2), only one item of
  // each of the above types may be included in the sent message.
  // Which means the kernel will merge them so we need one big buffer to get out fds
  constexpr auto controlBufSize = CMSG_SPACE(sizeof(int) * 3);
  char controlBuf[controlBufSize];

  // we only read ancillary data if we actually expect it, all other data will be discarded
  // and the OS will close them automatically
  if ( expectAncillary ) {
    msgh.msg_control     = controlBuf;
    msgh.msg_controllen  = controlBufSize;
  } else {
    msgh.msg_control     = nullptr;
    msgh.msg_controllen  = 0;
  }

  const auto receivedBytes = zyppng::eintrSafeCall( ::recvmsg, _socket, &msgh, 0 );
  if ( receivedBytes <= 0 ) {
    _readBuffer.chop( bytes );
    return receivedBytes;
  }

  if ( receivedBytes < bytes )
    _readBuffer.chop( bytes - receivedBytes );

  for ( auto hdr = CMSG_FIRSTHDR( &msgh ); hdr != nullptr; hdr = CMSG_NXTHDR( &msgh, hdr ) ) {
    if ( hdr->cmsg_level != SOL_SOCKET ||
         hdr->cmsg_type != SCM_RIGHTS ) {
      continue;
    }

    // check if the payload is a set of int's
    const auto payload = hdr->cmsg_len - CMSG_LEN(0);
    if ( payload % sizeof(int) != 0 ) {
      std::cerr << "Payload has bogus size" << std::endl;
      continue;
    }

    int *data = reinterpret_cast<int *>( CMSG_DATA(hdr) );
    for ( int i = 0; (ulong)i < ( payload / sizeof(int) ); i++ ) {
      std::cerr << "Received a fd in state "<< _state << ", expected:" << expectAncillary << std::endl;
      if ( _receivedFDs.size() < 3 )
        _receivedFDs.push_back( data[i] );
      else {
        std::cerr << "Closing FD because we received more than 3" << std::endl;
        ::close( data[i] );
      }
    }
  }
  return receivedBytes;
}

void ClientConnection::onReadyWrite() {

  const auto nwrite = _writeBuffer.frontSize();
  if ( !nwrite ) {
    allBytesWritten();
    return;
  }

  const auto nBuf = _writeBuffer.front();
  const auto written = zyppng::eintrSafeCall( ::send, _socket, nBuf, nwrite, MSG_NOSIGNAL );
  if ( written == -1 ) {
    if ( errno == EAGAIN || errno == EWOULDBLOCK )
      return;

    // complain only if the connection was not reset
    if ( errno != ECONNRESET && errno != EPIPE )
      std::cerr << "Socket received error " << errno << " " << zyppng::strerr_cxx( errno ) << std::endl;
    closeSocket();
  }

  // see if everything was written
  _writeBuffer.discard( written );
  if ( _writeBuffer.size() ) {
    _notify->setMode( zyppng::SocketNotifier::Write | zyppng::SocketNotifier::Error );
  } else {
    allBytesWritten();
  }
}

void ClientConnection::onSocketError() {
  closeSocket();
}

void ClientConnection::closeSocket() {
  _notify->setEnabled( false );
  ::close( _socket );
  _socket = -1;
  _sigClosed.emit();
}

void ClientConnection::allBytesWritten()
{
  if ( _state == ErrorState ) {
    closeSocket();
  } else if ( _state == ProcessingRequest ) {
    prepareForNextRequest();
  } else {
    std::cerr << "Reveiced allBytesWritten in weird state " << _state << std::endl;
  }
}

