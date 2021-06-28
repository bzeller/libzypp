/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "networkhandler_p.h"
#include <zypp/zyppng/media/Device>
#include <zypp/media/MediaException.h>
#include <zypp-core/zyppng/io/Socket>
#include <zypp-core/zyppng/rpc/rpc.h>
#include <zypp-proto/envelope.pb.h>
#include <zypp-proto/messages.pb.h>

#include <zypp/zyppng/media/private/medianetworkserver_p.h>
#include <zypp/zyppng/media/asyncdeviceop.h>
#include <zypp/zyppng/media/network/downloadspec.h>
#include <zypp/zyppng/media/network/private/networkrequesterror_p.h>

#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/zyppng/meta/TypeTraits>

#include <zypp/zyppng/media/network/private/mediadebug_p.h>
#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::NetworkDeviceHandler"

namespace {
  constexpr std::string_view DEVTYPE_NAME("network");
}

namespace zyppng
{
  using namespace zyppng::operators;

  class MediaNetworkGetOp : public AsyncDeviceOp<expected<PathRef>, NetworkDeviceHandler>
  {
    public:
      MediaNetworkGetOp ( zypp::proto::Request &&proto , std::shared_ptr<NetworkDeviceHandler> parent )
        : AsyncDeviceOp( parent )
        , _proto( std::move(proto) ) {
      }

      zypp::proto::Request & proto() {
        return _proto;
      }

      const zypp::proto::Request & proto() const {
        return _proto;
      }

      void started () {


      }

      void setFinished ( zypp::proto::DownloadFin &&fin ) {
        _parentHandler->removeRequest( _proto.requestid() );

        using Ret = expected<PathRef>;
        if ( !fin.has_error() ) {
          PathRef file( _parentHandler, zypp::Pathname( _proto.spec().targetpath() ) );
          return setReady( Ret::success( std::move(file) ) );
        }

        const auto &err = fin.error();
        if ( err.error() != NetworkRequestError::NoError ) {
          Url reqUrl = err.extra_info().at("requestUrl");
          switch ( err.error() )
          {
            case NetworkRequestError::Unauthorized: {
              std::string hint = err.extra_info().at("authHint");
              return setReady ( Ret::error(ZYPP_EXCPT_PTR(
                zypp::media::MediaUnauthorizedException(
                  reqUrl, err.errordesc(), err.nativeerror(), hint
                )
              )) );
              break;
            }
            case NetworkRequestError::TemporaryProblem:
              return setReady ( Ret::error(ZYPP_EXCPT_PTR(
                zypp::media::MediaTemporaryProblemException(reqUrl)
              )) );
              break;
            case NetworkRequestError::Timeout:
              return setReady ( Ret::error(ZYPP_EXCPT_PTR(
                zypp::media::MediaTimeoutException(reqUrl)
              )) );
              break;
            case NetworkRequestError::Forbidden:
              return setReady ( Ret::error(ZYPP_EXCPT_PTR(
                zypp::media::MediaForbiddenException(reqUrl, err.errordesc())
              )) );
              break;
            case NetworkRequestError::NotFound:
              return setReady ( Ret::error(ZYPP_EXCPT_PTR(
                zypp::media::MediaFileNotFoundException( reqUrl, _proto.spec().targetpath() )
              )) );
              break;
            case NetworkRequestError::ExceededMaxLen:
              return setReady ( Ret::error(ZYPP_EXCPT_PTR(
                zypp::media::MediaFileSizeExceededException( reqUrl, 0 )
              )) );
              break;
            default:
              break;
          }

          return setReady ( Ret::error(ZYPP_EXCPT_PTR(
            zypp::media::MediaCurlException( reqUrl, err.errordesc(), err.nativeerror() )
          )) );
        }

        return setReady ( Ret::error(ZYPP_EXCPT_PTR(
          zypp::media::MediaCurlException( _proto.spec().url(), err.errordesc() , "" )
        )) );
      }

      void setProgress ( const zypp::proto::DownloadProgress &prog ) {

      }

     // AsyncOpBase interface
    protected:
     void handlerShutdown() override {

     }

    private:
      zypp::proto::Request _proto;
      std::optional<zypp::proto::DownloadFin> _result;

  };


  NetworkDeviceType::NetworkDeviceType(DeviceManager &devMgr)
        : DeviceType( devMgr, DEVTYPE_NAME.data(), true )
  { }

  std::vector<std::shared_ptr<Device> > NetworkDeviceType::detectDevices(const zypp::Url &url, const std::vector<std::string> &filters)
  {
    if ( _sysDevs.count( url ) ) {
      return { _sysDevs[url] };
    }

    auto nDev = std::make_shared<Device>( type(), url.asString(), 0, 0 );
    auto &props = nDev->properties();
    props.insert( std::make_pair("baseurl", url) );
    _sysDevs.insert( std::make_pair(url, nDev) );
    return { nDev };
  }

  expected<std::shared_ptr<DeviceHandler> > NetworkDeviceType::attachDevice( std::shared_ptr<Device> dev, const zypp::Url &baseurl, const zypp::filesystem::Pathname &attachPoint )
  {
    using Ret = expected< std::shared_ptr<DeviceHandler> >;

    if ( !deviceManager().isUseableAttachPoint( attachPoint ) ) {
      return Ret::error( ZYPP_EXCPT_PTR( zypp::media::MediaBadAttachPointException( baseurl) ) );
    }

    auto socket = zyppng::Socket::create( AF_UNIX, SOCK_STREAM, 0 );
    auto addr   = std::make_shared<zyppng::UnixSockAddr>( MediaNetworkThread::instance().sockPath(), true );

    int errs = 0;
    while ( true ) {
      socket->connect( addr );
      if ( !socket->waitForConnected() ) {
        if ( socket->lastError() == Socket::ConnectionRefused )
          continue;
        ERR << "Got socket error" << socket->lastError()<< std::endl;
        std::this_thread::sleep_for( std::chrono::seconds(1) );
        errs++;
        if ( errs <= 10 )
          continue;

        socket.reset();
      }
      break;
    }

    if ( !socket )
      return Ret::error( ZYPP_EXCPT_PTR( zypp::media::MediaException("Failed to connect to backend") ) );

    //taking ownership of the attachpoint
    DeviceAttachPointRef ref( attachPoint, zypp::filesystem::recursive_rmdir );
    auto handler = std::shared_ptr<NetworkDeviceHandler>( new NetworkDeviceHandler ( std::move(socket), *dev, baseurl, deviceManager() ) );
    handler->setAttachPoint( std::move(ref) );
    return Ret::success( handler );
  }

  expected<void> NetworkDeviceType::ejectDevice(Device &dev)
  {
    return prepareEject( dev );
  }

  bool NetworkDeviceType::canHandle(const zypp::Url &url)
  {
    const auto &scheme = url.getScheme();
    return ( scheme == "ftp" || scheme == "tftp" || scheme == "http" || scheme == "https" );
  }

  std::vector< std::weak_ptr<DeviceHandler> > NetworkDeviceType::activeHandlers()
  {
    std::vector<std::weak_ptr<DeviceHandler>> res;
    std::for_each( _sysDevs.begin(), _sysDevs.end(), [ &res ]( const auto &dev ){
      const auto &devHandlers = dev.second->activeHandlers();
      res.insert( res.end(), devHandlers.begin(), devHandlers.end() );
    });
    return res;
  }

  NetworkDeviceHandler::NetworkDeviceHandler(std::shared_ptr<Socket> &&backendSocket, Device &dev, const zypp::Url &baseUrl, DeviceManager &parent )
    : DeviceHandler( dev, baseUrl, baseUrl.getPathName(), parent )
    , _socket( std::move(backendSocket) )
  {
    connect( *_socket, &Socket::sigReadyRead, *this, &NetworkDeviceHandler::socketReadyRead );
  }

  NetworkDeviceHandler::~NetworkDeviceHandler()
  {
    // The device handler should never be released as long as there are still requests running, because they are referencing
    // the handler. So there is no need for us to be releasing requests by hand.
    DBG_MEDIA << "NetworkDeviceHandler released, with " << _requests.size() << " nr or requests still running" << std::endl;
    _socket->close();
  }

  AsyncOpRef<expected<std::list<std::string>>> NetworkDeviceHandler::getDirInfo( const zypp::Pathname & dirname, bool dots)
  {
    return getDirInfoYast( dirname, dots ) | mbind( []( zypp::filesystem::DirContent &&list ) -> expected<std::list<std::string>> {
      std::list<std::string> retlist;
      for ( zypp::filesystem::DirContent::const_iterator it = list.begin(); it != list.end(); ++it )
          retlist.push_back( it->name );
      return expected<std::list<std::string>>::success( std::move(retlist) );
    }) ;
  }

  AsyncOpRef<expected<zypp::filesystem::DirContent>> NetworkDeviceHandler::getDirInfoExt( const zypp::Pathname &dirname, bool dots)
  {
    return getDirInfoYast( dirname, dots );
  }

  AsyncOpRef<expected<PathRef>> NetworkDeviceHandler::getDir( const zypp::Pathname & dirname, bool recurse_r )
  {
  }

  AsyncOpRef<expected<PathRef>> NetworkDeviceHandler::getFile( const zypp::OnMediaLocation &file )
  {
    return makeGetFile( file, false );
  }

  AsyncOpRef<expected<PathRef>> NetworkDeviceHandler::makeGetFile( const zypp::OnMediaLocation &file, bool checkOnly )
  {
    if ( !_socket ) {
      return makeReadyResult<expected<PathRef>>( expected<PathRef>::error(ZYPP_EXCPT_PTR( zypp::media::MediaException("Backend not connected"))) );
    }

    return makeRequest( file, checkOnly )
           | mbind( [this, checkOnly]( zypp::proto::Request &&r ) {
               if ( !checkOnly ) {
                 zypp::Pathname dest(r.spec().targetpath());
                 if( zypp::filesystem::assert_dir( dest.dirname() ) ){
                   DBG << "assert_dir " << dest.dirname() << " failed" << std::endl;
                   return expected<zypp::proto::Request>::error(
                       ZYPP_EXCPT_PTR( zypp::media::MediaSystemException( zypp::Url(r.spec().url()), "System error on " + dest.dirname().asString()) )
                     );
                 }
               }
               return startRequest( std::move(r) );
             })
           | mbind( [this]( auto &&proto ) -> AsyncOpRef<expected<PathRef>> {
              auto id = proto.requestid();
              auto op = std::make_shared<MediaNetworkGetOp>( std::move(proto), shared_this<NetworkDeviceHandler>() );
              _requests.insert( std::make_pair(id, op) );
              return op;
             });
  }

  AsyncOpRef<expected<bool>> NetworkDeviceHandler::checkDoesFileExist( const zypp::Pathname & filename )
  {
    return makeGetFile( zypp::OnMediaLocation(filename), true )
      | mbind( []( PathRef && ){
        return expected<bool>::success(true);
      });
  }

  void NetworkDeviceHandler::socketReadyRead()
  {
    const auto &cancelReqWithError = []( auto &r, const std::string &reason, const auto e ){
      zypp::proto::NetworkRequestError err;
      err.set_error( e );
      err.set_errordesc( reason );
      zypp::proto::DownloadFin f;
      *f.mutable_error() = std::move(err);
      f.set_requestid( r->proto().requestid() );
      r->setFinished( std::move(f) );
    };

    try {
      while ( _socket->canRead() ) {
        if ( !_pendingMessageSize ) {
          // if we cannot read the message size wait some more
          if ( _socket->bytesAvailable() < sizeof( rpc::HeaderSizeType ) )
            return;

          rpc::HeaderSizeType msgSize;
          _socket->read( reinterpret_cast<char *>( &msgSize ), sizeof( decltype (msgSize) ) );
          _pendingMessageSize = msgSize;
        }

        // wait for the full message size to be available
        if ( _socket->bytesAvailable() < static_cast<size_t>( *_pendingMessageSize ) )
          return;

        ByteArray message ( *_pendingMessageSize, '\0' );
        _socket->read( message.data(), *_pendingMessageSize );
        _pendingMessageSize.reset();

        zypp::proto::Envelope e;
        if (! e.ParseFromArray( message.data(), message.size() ) ) {
          //abort, we can not recover from this. Bytes might be mixed up on the socket
          ZYPP_THROW( zypp::media::MediaException("NetworkDeviceHandler backend connection broken.") );
        }
        const auto &mName = e.messagetypename();
        if ( mName == "zypp.proto.DownloadStart" ) {
          zypp::proto::DownloadStart st;
          st.ParseFromString( e.value() );

          auto r = findRequest( st.requestid() );
          if ( !r )
            return;

          MIL_MEDIA << "Received the download started event for file "<< zypp::Url( r->proto().spec().url() ) <<" from server." << std::endl;
          r->started();

        } else if ( mName == "zypp.proto.DownloadProgress" ) {
          zypp::proto::DownloadProgress prog;
          prog.ParseFromString( e.value() );

          auto r = findRequest( prog.requestid() );
          if ( !r )
            return;

          std::cout << "Received progress for download: " << zypp::Url( r->proto().spec().url() ) << " " << prog.now() << "/" << prog.total() << " " << std::endl;
          r->setProgress( prog );

        } else if ( mName == "zypp.proto.DownloadFin" ) {
          zypp::proto::DownloadFin fin;
          fin.ParseFromString( e.value() );

          auto r = findRequest( fin.requestid() );
          if ( !r )
            return;

          MIL_MEDIA << "Received FIN for download: " << zypp::Url( r->proto().spec().url() ) << std::endl;
          r->setFinished( std::move(fin) );
        } else  if ( mName == "zypp.proto.Status" ) {

          // Status messages are only received after requesting a new download to the server,
          // if that fails, we need to cancel the existing download

          zypp::proto::Status s;
          s.ParseFromString( e.value() );

          if ( s.code() != zypp::proto::Status::Ok ) {
            auto r = findRequest( s.requestid() );
            if (r) {
              WAR_MEDIA << "Received Status with code: " << s.code() << zypp::Url( r->proto().spec().url() ) << std::endl;
              cancelReqWithError( r, "Failed to register download in the backend.", NetworkRequestError::InternalError );
            }
          } else {
            MIL_MEDIA << "Download with id " << s.requestid() << " was ack'ed" << std::endl;
          }
        }  else {
          // error , unknown message
          WAR_MEDIA << "Received unexpected message... ignoring" << std::endl;
        }

      }
    }  catch ( zypp::Exception &e ) {
      // we cannot let the exception travel through the event loop, instead we remember it for later
      ZYPP_CAUGHT( e );
      ERR << "Catched exception " << e << " while reading from the socket, disconnecting from backend." << std::endl;
      while( _requests.size() ) {
        auto i = _requests.begin();
        if ( i == _requests.end() )
          break;
        auto ptr = i->second.lock();
        if ( !ptr ) {
          _requests.erase(i);
          continue;
        }
        cancelReqWithError( ptr, "Backend connection broke.", NetworkRequestError::InternalError );
      }
      _socket->abort();
      _socket.reset();
    }
  }

  /*!
    Only called by the AsyncOps when they are finished, just to be removed
    from the internal registry
   */
  void NetworkDeviceHandler::removeRequest( RequestId reqId )
  {
    if ( _requests.count( reqId ) )
      _requests.erase( reqId );
  }

  std::shared_ptr<MediaNetworkGetOp> NetworkDeviceHandler::findRequest ( RequestId rId )
  {
    if ( _requests.count(rId) ) {
      auto locked = _requests[rId].lock();
      if ( locked )
        return locked;

      DBG_MEDIA << "Found stale request in internal map, removing it" << std::endl;
      _requests.erase( rId );
    }
    return {};
  }

  expected<zypp::proto::Request> NetworkDeviceHandler::makeRequest( const zypp::OnMediaLocation &loc, bool checkOnly )
  {
    DBG << loc.filename().asString() << std::endl;

    const auto &_url = this->url();
    if( !_url.isValid() )
      return expected<zypp::proto::Request>::error( ZYPP_EXCPT_PTR(zypp::media::MediaBadUrlException( _url )) );

    if( _url.getHost().empty() )
      return expected<zypp::proto::Request>::error( ZYPP_EXCPT_PTR(zypp::media::MediaBadUrlEmptyHostException(_url)) );

    zypp::Url url( getFileUrl(loc.filename()) );

    DownloadSpec spec( url, localPath(loc.filename()).absolutename(), loc.downloadSize() );
    spec.setTransferSettings( _settings )
      .setHeaderSize( loc.headerSize() )
      .setHeaderChecksum( loc.headerChecksum() )
      .setDeltaFile( loc.deltafile() )
      .setCheckExistsOnly( checkOnly );

    zypp::proto::Request fileReq;
    fileReq.set_requestid( ++_nextRequestId  );
    *fileReq.mutable_spec() = std::move( spec.protoData() );
    fileReq.set_streamprogress( true );
    return expected<zypp::proto::Request>::success(fileReq);
  }

  template <typename Req>
  expected<Req> NetworkDeviceHandler::startRequest( Req &&req )
  {
    if ( !_socket ) {
      return expected<Req>::error( ZYPP_EXCPT_PTR(zypp::media::MediaException("Backend not connected")) );
    }

    zypp::proto::Envelope env;

    env.set_messagetypename( req.GetTypeName() );
    req.SerializeToString( env.mutable_value() );

    DBG << "Preparing to send message: " << env.messagetypename() << " " << env.value().size() << std::endl;

    const auto &str = env.SerializeAsString();
    rpc::HeaderSizeType msgSize = str.length();
    _socket->write( (char *)(&msgSize), sizeof( rpc::HeaderSizeType ) );
    _socket->write( str.data(), str.size() );
    return expected<Req>::success( std::move(req) );
  }

  expected<zypp::proto::CancelDownload> NetworkDeviceHandler::cancelRequest(RequestId reqId)
  {
    if ( !_socket ) {
      return expected<zypp::proto::CancelDownload>::error( ZYPP_EXCPT_PTR(zypp::media::MediaException("Backend not connected")) );
    }
    zypp::proto::CancelDownload cl;
    cl.set_requestid( reqId );
    return startRequest( std::move(cl) ) ;
  }

  zypp::Url NetworkDeviceHandler::getFileUrl( const zypp::Pathname & filename_r ) const
  {
    // Simply extend the URLs pathname. An 'absolute' URL path
    // is achieved by encoding the leading '/' in an URL path:
    //   URL: ftp://user@server		-> ~user
    //   URL: ftp://user@server/		-> ~user
    //   URL: ftp://user@server//		-> ~user
    //   URL: ftp://user@server/%2F	-> /
    //                         ^- this '/' is just a separator
    zypp::Url newurl( url() );
    newurl.setPathName( ( zypp::Pathname("./"+url().getPathName()) / filename_r ).asString().substr(1) );
    return newurl;
  }




}
