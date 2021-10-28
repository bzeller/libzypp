/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPPNG_MEDIA_HANDLER_NETWORKHANDLER_P_H_INCLUDED
#define ZYPPNG_MEDIA_HANDLER_NETWORKHANDLER_P_H_INCLUDED

#include <zypp/zyppng/media/DeviceManager>
#include <zypp/zyppng/media/DeviceHandler>
#include <zypp/media/TransferSettings.h>

#include <unordered_map>

namespace zypp::proto {
  class Request;
  class CancelDownload;
}

namespace zyppng {

  class Socket;
  class MediaNetworkGetOp;

  using RequestId = uint32_t;

  class NetworkDeviceType : public DeviceType
  {
    // DeviceType interface
  public:
    NetworkDeviceType( DeviceManager &devMgr );
    std::vector<std::shared_ptr<Device> > detectDevices(const zypp::Url &url, const std::vector<std::string> &filters) override;
    expected<std::shared_ptr<DeviceHandler> > attachDevice(std::shared_ptr<Device> dev, const zypp::Url &baseurl, const zypp::filesystem::Pathname &attachPoint) override;
    expected<void> ejectDevice(Device &dev) override;
    bool canHandle(const zypp::Url &url) override;
    std::vector< std::weak_ptr<DeviceHandler> > activeHandlers () override;

  private:
    std::vector< std::shared_ptr<Device> > _knownDevices;

    // The device key is the base url
    using DevKey = zypp::Url;
    std::map< DevKey, std::shared_ptr<Device> > _sysDevs;
    bool _detectedSysDevs = false; // < did we already execute the auto detection
  };

  class NetworkDeviceHandler : public DeviceHandler
  {
    friend class MediaNetworkGetOp;

    public:
      NetworkDeviceHandler( std::shared_ptr<Socket> &&backendSocket, Device &dev, const zypp::Url &baseUrl, DeviceManager &parent );
      ~NetworkDeviceHandler() override;

      // DeviceHandler interface
      bool isIdle() const override;
    protected:
      AsyncOpRef<expected<std::list<std::string>>> getDirInfo( const zypp::Pathname & dirname, bool dots = true ) override;
      AsyncOpRef<expected<zypp::filesystem::DirContent>> getDirInfoExt( const zypp::Pathname &dirname, bool dots = true ) override;
      AsyncOpRef<expected<PathRef>> getDir( const zypp::Pathname & dirname, bool recurse_r ) override;
      AsyncOpRef<expected<PathRef>> getFile( const zypp::OnMediaLocation &file ) override;
      AsyncOpRef<expected<bool>> checkDoesFileExist( const zypp::Pathname & filename ) override;

    private:
      void socketReadyRead ();
      void removeRequest ( RequestId reqId );
      std::shared_ptr<MediaNetworkGetOp> findRequest ( RequestId rId );
      expected<zypp::proto::Request> makeRequest ( const zypp::OnMediaLocation &loc, bool checkOnly = false );
      AsyncOpRef<expected<PathRef>> makeGetFile( const zypp::OnMediaLocation &file, bool checkOnly );

      template <typename Req>
      expected<Req> startRequest( Req &&req );

      expected<zypp::proto::CancelDownload> cancelRequest( RequestId reqId );
      zypp::Url getFileUrl(const zypp::Pathname &filename_r) const;

    private:
      RequestId               _nextRequestId = 0;
      std::shared_ptr<Socket> _socket;
      std::optional<int32_t>  _pendingMessageSize;
      zypp::media::TransferSettings _settings;
      std::unordered_map<RequestId, std::weak_ptr<MediaNetworkGetOp> > _requests;
  };
}



#endif
