#ifndef CLIENTCONNECTION_H
#define CLIENTCONNECTION_H

#include <zypp-proto/envelope.pb.h>
#include <zypp-proto/zypp-spawn.pb.h>

#include <zypp-core/zyppng/io/private/iobuffer_p.h>
#include <zypp-core/zyppng/io/private/socket_p.h>
#include <zypp-core/zyppng/io/private/zyppspawnengine_p.h>

class Server;

using HeaderSizeType = zyppng::ZyppSpawnEngine::HeaderSizeType;

class ClientConnection : public zyppng::Base {

public:

  enum State {
    WaitForHeader,
    ReceivingRequest,
    ProcessingRequest,
    ErrorState,
  };

  ClientConnection( Server &srv, zyppng::Socket::Ptr sock );
  ~ClientConnection();

  zyppng::SignalProxy<void()> sigClosed ();


protected:
  void onSocketActivated ( const zyppng::SocketNotifier &, int event );
  void onBytesAvailable ();

  void cleanupUnusedFds ();
  void handleMessage (const zypp::proto::Envelope & m );
  void prepareForNextRequest ();
  void handleMalformedMessage ();

  size_t readBytesFromSocket (bool expectAncillary = false, size_t bytesToRead = 0 );

  template< typename T >
  void sendMessage( T &m )
  {
    zypp::proto::Envelope env;
    env.set_messagetypename( m.GetTypeName() );
    m.SerializeToString( env.mutable_value() );

    //DBG << "Sending message\n" << env.DebugString() << std::endl;

    std::string data = env.SerializeAsString();
    const HeaderSizeType size = data.size();

    _writeBuffer.append( reinterpret_cast<const char *>( &size ), sizeof(decltype(size)) );
    _writeBuffer.append(  data.data(), size );

    onReadyWrite();
  }

  void onReadyWrite ();
  void onSocketError ();
  void closeSocket ();
  void allBytesWritten ();

private:
  zyppng::SocketNotifier::Ptr _notify = nullptr;
  zyppng::IOBuffer _writeBuffer;
  zyppng::IOBuffer _readBuffer;
  int _socket;
  State _state = WaitForHeader;

  Server *_server = nullptr;
  std::vector<int> _receivedFDs;
  std::optional<int32_t> _pendingMessageSize;
  zyppng::Signal<void()> _sigClosed;

};

#endif // CLIENTCONNECTION_H
