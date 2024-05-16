/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "io.h"
#include <zypp-core/base/String.h>
#include <variant>

namespace zyppng::io {

  struct DeviceReader : public AsyncOp<expected<ByteArray>>
  {

    DeviceReader( IODeviceRef &&device, uint channel, const char delim )
      : _device( std::move(device) )
      , _channel(channel)
      , _limit(delim) {}

    DeviceReader( IODeviceRef &&device, uint channel, const int64_t bytes )
      : _device( std::move(device) )
      , _channel(channel)
      , _limit(bytes) {}

    static AsyncOpRef<expected<ByteArray>> execute( IODeviceRef device, uint channel, const char delim ) {
      auto self = std::make_shared<DeviceReader>( std::move(device), channel, delim );
      self->run();
      return self;
    }

    static AsyncOpRef<expected<ByteArray>> execute( IODeviceRef device, uint channel, const int64_t bytes ) {
      auto self = std::make_shared<DeviceReader>( std::move(device), channel, bytes );
      self->run();
      return self;
    }

  private:

    void run() {
      connect( *_device, &IODevice::sigReadyRead, *this, &DeviceReader::on_readyRead );
      connect( *_device, &IODevice::sigReadChannelFinished, *this, &DeviceReader::on_readChanFinished );
      on_readyRead();
    }

    void on_readyRead() { readAvailableData(); }

    bool readAvailableData() {
      if ( _limit.index () == 0 ) {
        auto delim = std::get<0>(_limit);
        if ( _device->canReadUntil( _channel, delim ) ) {
          setReady( expected<ByteArray>::success( _device->channelReadUntil( _channel, delim ) ) );
          return true;
        }
      } else {
        auto maxRead = std::get<1>(_limit);
        if ( _device->bytesAvailable () >= maxRead ) {
          setReady( expected<ByteArray>::success( _device->read( maxRead ) ) );
          return true;
        }
      }
      return false;
    }

    void on_readChanFinished( uint channel ) {
      if ( channel != _channel )
        return;
      if ( !readAvailableData () ) {
        setReady( expected<ByteArray>::error( ZYPP_EXCPT_PTR (IoException( "Device closed before requested data was received." ) )  ) );
        return;
      }
    }

    IODeviceRef _device;
    uint _channel;
    std::variant<char, int64_t> _limit;
  };

  expected<ByteArray> readline( std::istream & stream )
  {
    return read_until ( stream, '\n' );
  }

  AsyncOpRef<expected<ByteArray>> readline( IODeviceRef device )
  {
    return read_until ( device, '\n' );
  }

  expected<ByteArray> read( std::istream & str , int64_t bytes )
  {
    std::ostringstream datas;

    int64_t bytesRead = 0;
    while ( bytesRead < bytes ) {
      char ch = 0;
      if ( !str.get( ch ) ) {
        break;
      }
      datas.put( ch );
    }

    if ( bytesRead != bytes ) {
      return expected<ByteArray>::error( ZYPP_EXCPT_PTR (IoException( "Unable to read requested data." ) ) );
    }

    const auto &data = datas.str();
    return expected<ByteArray>::success( data.data(), data.length() );
  }

  AsyncOpRef<expected<ByteArray>> read(IODeviceRef device , int64_t bytes)
  {
    return DeviceReader::execute ( std::move(device), device->currentReadChannel(), bytes );
  }

  expected<ByteArray> read_until( std::istream & stream, const char delim )
  {
    const auto &data = zypp::str::receiveUpTo( stream, delim );
    if ( ! stream.good() )
      return expected<ByteArray>::error( ZYPP_EXCPT_PTR (IoException( "Missing delimiter in stream" ) ) );

    return expected<ByteArray>::success( data.c_str(), data.length() );
  }

  AsyncOpRef<expected<ByteArray>> read_until(IODeviceRef device , const char delim)
  {
    if ( !device || !device->isOpen() || !device->canRead() )
      return makeReadyResult( expected<ByteArray>::error( ZYPP_EXCPT_PTR (IoException( "Bad device, or device is not readable" ) ) ) );

    if ( device->canReadUntil( device->currentReadChannel(), delim ) ) {
      return makeReadyResult( expected<ByteArray>::success( device->channelReadUntil( device->currentReadChannel(), delim ) ) );
    }

    // no delim in the device buffer , we need to wait
    return DeviceReader::execute ( std::move(device), device->currentReadChannel(), '\n' );
  }
}
