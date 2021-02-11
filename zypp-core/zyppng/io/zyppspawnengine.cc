#include "private/zyppspawnengine_p.h"
#include "private/forkspawnengine_p.h"
#include <optional>

#include <zypp-proto/envelope.pb.h>
#include <zypp-proto/zypp-spawn.pb.h>
#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>
#include <zypp-core/zyppng/io/SockAddr>
#include <zypp-core/base/String.h>
#include <zypp-core/ExternalProgram.h>
#include <zypp/base/LogControl.h>
#include <zypp/base/Gettext.h>
#include <zypp-core/AutoDispose.h>
#include <glib.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mutex>


constexpr std::string_view ZYPP_SPAWN_EXECUTABLE("/usr/lib/zypp/tools/zypp-spawn");

namespace zyppng {

  namespace  {
    std::mutex &startSpawnerLock () {
      static std::mutex mut;
      return mut;
    }

    bool ensureSpawner( int sockFD, const zyppng::UnixSockAddr &addr ) {

      static pid_t procId = -1;

      {
        std::lock_guard lock( startSpawnerLock() );

        // if we get here for some reason we were not able to connect in the first place
        // lets wait on the pid if its a valid one to make sure the process did not die before
        // or if it was just started while we were waiting on the lock.
        if ( procId != -1 ) {
          int status = 0;
          int p = zyppng::eintrSafeCall( ::waitpid, procId, &status, WNOHANG );
          if ( p != 0 ) {
            ERR << "zypp-spawn was running but seems to have vanished." << std::endl;
            if ( p > 0 ) {
              zypp::AutoDispose<GError *> err( nullptr, g_error_free );
              if ( !g_spawn_check_exit_status( status, &*err ) ) {
                ERR << "zypp-spawn exited ( "<< (*err)->message << ")" << std::endl;
              } else {
                ERR << "zypp-spawn exited with exit code: 0." << std::endl;
              }
            }
            procId = -1;
          }
        }

        if ( procId == -1 ) {
          // the process is not yet there, lets start it
          DBG << "Starting the zypp-spawn process because it does not exist" << std::endl;

          const auto pidStr = zypp::str::asString( ::getpid() );
          const char *argv[] {
#ifdef ZYPP_OVERRIDE_SPAWN_EXECUTABLE
            ZYPP_OVERRIDE_SPAWN_EXECUTABLE,
#else
            ZYPP_SPAWN_EXECUTABLE.data(),
#endif            
            pidStr.c_str(),
            nullptr
          };

          zypp::AutoFD stdErrRedir( ::open( "/dev/null" , O_RDWR ) );
          GlibSpawnEngine ext;
          ext.setDieWithParent( true );
          ext.setUseDefaultLocale( true );
          ext.start ( argv, -1, stdErrRedir, stdErrRedir );
          if ( !ext.isRunning( false ) )
            return false;

          procId = ext.pid();
        }

        if ( procId == -1 )
          return false;
      }

      // now try to connect again
      bool res = zyppng::trySocketConnection( sockFD, addr, 1000 );
      if ( !res ) {
        return false;
      }

      return true;
    }

    template< typename T >
    bool sendMessage( int sock, T &m, struct msghdr *msgh = nullptr )
    {
      zypp::proto::Envelope env;
      env.set_messagetypename( m.GetTypeName() );
      m.SerializeToString( env.mutable_value() );

      //DBG << "Sending message\n" << env.DebugString() << std::endl;

      ByteArray data( env.ByteSizeLong(), '\0' );
      env.SerializeToArray( data.data(), data.size() );

      const zyppng::ZyppSpawnEngine::HeaderSizeType size = data.size();

      struct msghdr locHdr;
      if ( !msgh ) {
        msgh = &locHdr;
        msgh->msg_control = nullptr;
        msgh->msg_controllen = 0;
      }

      struct iovec iov;
      iov.iov_base = data.data();
      iov.iov_len  = data.size() * sizeof(char);

      msgh->msg_name = nullptr;
      msgh->msg_namelen = 0;
      msgh->msg_iov = &iov;
      msgh->msg_iovlen = 1;

      // send the message size with no ancillary data, so we have a clear boundary on the receiving side
      if ( zyppng::eintrSafeCall( ::send, sock, reinterpret_cast<const char *>(&size), sizeof( decltype (size) ), MSG_NOSIGNAL ) != sizeof( decltype (size) ) ) {
        return false;
      }

      // now send the ancillary as well
      if ( zyppng::eintrSafeCall( ::sendmsg, sock, msgh, MSG_NOSIGNAL ) != (ssize_t)( data.size() * sizeof(char) ) ) {
        return false;
      }

      return true;
    }

    bool recvBytes ( int fd, char *buf, size_t n ) {
      size_t read = 0;
      while ( read != n ) {
        const auto r = zyppng::eintrSafeCall( ::recv, fd, buf + read , n - read , 0 );
        if ( r == -1 )
          return false;

        read += r;
      }
      return true;
    }

    bool receiveEnvelope ( int sock, zypp::proto::Envelope &m ) {
      using HeaderSizeType = zyppng::ZyppSpawnEngine::HeaderSizeType;
      HeaderSizeType size = 0;
      if (! recvBytes( sock, reinterpret_cast<char *>(&size), sizeof( HeaderSizeType ) ) ) {
        ERR << "Failed to read response from spawn server process." << std::endl;
        return false;
      }

      zyppng::ByteArray msgBuf( size, '\0' );
      if (! recvBytes( sock, msgBuf.data(), msgBuf.size() ) ) {
        ERR << "Failed to read response message from spawn server process." << std::endl;
        return false;
      }

      if (! m.ParseFromArray( msgBuf.data(), msgBuf.size() ) ) {
        //send error and close, we can not recover from this. Bytes might be mixed up on the socket
        ERR << "Failed to parse response message from spawn server process." << std::endl;
        return false;
      }
      return true;
    }

    template < typename T >
    std::optional<T> parseEnvelope ( const zypp::proto::Envelope &env ) {
      T msg;
      if ( env.messagetypename() == msg.GetTypeName() ) {
        if (!msg.ParseFromString( env.value() )) {
          return {};
        }
        return msg;
      }
      return {};
    }
  }

  ZyppSpawnEngine::~ZyppSpawnEngine()
  {
    if ( _pid != -1 && ensureConnection() ) {
      zypp::spawn::proto::ReleaseProcess rel;
      rel.set_pid( _pid );
      sendMessage ( *_sock, rel );
    }
  }

  bool ZyppSpawnEngine::start( const char * const *argv, int stdin_fd, int stdout_fd, int stderr_fd )
  {
    if ( !argv || !argv[0] ) {
      _execError = _("Invalid spawn arguments given.");
      _exitStatus = 128;
      return false;
    }

    if ( !ensureConnection() ) {
      ERR << "Unable to connect to zypp-spawn" << std::endl;
      _execError = _("Unable to connect to zypp-spawn");
      _exitStatus = 128;
      return false;
    }

    zypp::spawn::proto::SpawnProcess req;

    // since the spawn process was spawned early we need to send over our full env in order
    // to have the correct setup
    for( char **e = environ; *e != nullptr; e++ )
      *req.add_env( ) = *e;
    for (int i = 0; argv[i]; i++)
      req.add_argv( argv[i] );
    for ( const auto &e : _environment )
      *req.add_env() = e.first +"="+e.second;
    if ( _useDefaultLocale )
      *req.add_env() = "LC_ALL=C";
    if ( !_chroot.empty() )
      req.set_chroot( _chroot.asString() );
    if ( !_workingDirectory.empty() )
      req.set_chdir( _workingDirectory.asString() );
    req.set_diewithparent( _dieWithParent );
    req.set_resetprogressgroup( _switchPgid );

    std::vector<int> fdBuffer;
    fdBuffer.reserve(3);
    if ( stdin_fd != -1 ) {
      req.add_fdintrailer( zypp::spawn::proto::SpawnProcess::StdIn );
      fdBuffer.push_back( stdin_fd );
    }
    if ( stdout_fd != -1 ) {
      req.add_fdintrailer( zypp::spawn::proto::SpawnProcess::StdOut );
      fdBuffer.push_back( stdout_fd );
    }
    if ( stderr_fd != -1 ) {
      req.add_fdintrailer( zypp::spawn::proto::SpawnProcess::StdErr );
      fdBuffer.push_back( stderr_fd );
    }

    const auto ctrlLen =  CMSG_LEN( sizeof (int) * fdBuffer.size() );
    std::unique_ptr<char[]> controlBuf( new char[ ctrlLen ] );

    struct msghdr msgh;
    msgh.msg_control    = controlBuf.get();
    msgh.msg_controllen = ctrlLen;

    auto hdr = CMSG_FIRSTHDR(&msgh);
    hdr->cmsg_len = ctrlLen;
    hdr->cmsg_level = SOL_SOCKET;
    hdr->cmsg_type  = SCM_RIGHTS;
    memcpy( CMSG_DATA(hdr), fdBuffer.data(), sizeof(int) * fdBuffer.size() );

    if ( !sendMessage( _sock, req, &msgh ) ) {
      ERR << "Failed to send request to spawn server process." << std::endl;
      _execError = _("Failed to send request to spawn server process.");
      _exitStatus = 128;
      _sock.reset();
      return false;
    }

    zypp::proto::Envelope m;
    if ( !receiveEnvelope( _sock, m) ) {
      _execError = _("Failed to read request response from spawn-server process.");
      _exitStatus = 128;
      _sock.reset();
      return false;
    }

    if ( auto result = parseEnvelope<zypp::spawn::proto::SpawnSuccess>(m) ) {
      _pid = result->pid();
      DBG << "Spawned a process " << _pid << std::endl;
      return true;

    } else if ( auto result = parseEnvelope<zypp::spawn::proto::RequestFailed>(m) ) {

      _execError = result->error();
      _exitStatus = 128;
      return false;

    } else {
      ERR << "SpawnProcess returned invalid message '" << m.messagetypename() << "'." << std::endl;
      _execError = _("Invalid response from spawn-server process.");
      _exitStatus = 128;
      _sock.reset();
      return false;
    }
  }

  bool ZyppSpawnEngine::isRunning( bool wait )
  {
    if ( _pid == -1 )
      return false;

    if ( !ensureConnection() ) {
      ERR << "Unable to connect to zypp-spawn to wait for pid: " << _pid << std::endl;
      return false;
    }

    zypp::spawn::proto::WaitPid wPid;
    wPid.set_pid( _pid );
    wPid.set_wait( wait );
    if ( !sendMessage( _sock, wPid ) ) {
      ERR << "Unable to communicate with zypp-spawn to wait for pid: " << _pid << std::endl;
      _sock.reset();
      return false;
    }

    zypp::proto::Envelope response;
    if ( !receiveEnvelope( _sock, response ) ) {
      ERR << "Unable to reveive a answer from zypp-spawn to wait for pid: " << _pid << std::endl;
      _sock.reset();
      return false;
    }

    if ( auto result = parseEnvelope<zypp::spawn::proto::WaitPidResult>(response) ) {

      if ( result->ret() == 0 )
        return true; // still running

      // Here: completed...
      _exitStatus = checkStatus( result->status() );
      _pid = -1;
      _sock.reset(); // close connection , new process new socket
      return false;

    } else if ( auto result = parseEnvelope<zypp::spawn::proto::RequestFailed>(response) ) {
      ERR << "waitpid( " << _pid << ") returned error '" << result->error() << "'" << std::endl;
      _sock.reset();
      return false;
    } else {
      ERR << "waitpid( " << _pid << ") returned invalid message '" << response.messagetypename() << "'." << std::endl;
      _sock.reset();
      return false;
    }
  }

  bool ZyppSpawnEngine::ensureConnection()
  {
    if ( _sock != -1 )
      return true;

    static const std::string sockname( zypp::str::Format(ZyppSpawnEngine::sockNameTemplate.data() ) % ::getpid() );

    _sock = zypp::AutoFD ( ::socket( AF_UNIX, SOCK_STREAM, 0 ) );
    if ( _sock == -1 )
      return false;

    zyppng::UnixSockAddr addr( sockname, true );

    // first we try to connect without timeout
    int connRes = zyppng::eintrSafeCall( ::connect, _sock, addr.nativeSockAddr(), addr.size() );
    if ( connRes == -1 ) {
      if ( errno == ECONNREFUSED ) {
        // the process is not yet there, lets start it
        if ( ensureSpawner( _sock, addr ) )
          return true;
      }

      // close the socket something went wrong
      _sock.reset();
      return false;
    }
    return true;
  }
}
