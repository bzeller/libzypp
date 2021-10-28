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
#ifndef ZYPPNG_MEDIA_HANDLER_CDROMHANDLER_P_H_INCLUDED
#define ZYPPNG_MEDIA_HANDLER_CDROMHANDLER_P_H_INCLUDED

#include <zypp/zyppng/media/DeviceManager>
#include <zypp/zyppng/media/DeviceHandler>

namespace zyppng {

  class CDRomDeviceType : public DeviceType
  {
    // DeviceType interface
  public:
    CDRomDeviceType( DeviceManager &devMgr );
    std::vector<std::shared_ptr<Device> > detectDevices(const zypp::Url &url, const std::vector<std::string> &filters) override;
    expected<std::shared_ptr<DeviceHandler> > attachDevice(std::shared_ptr<Device> dev, const zypp::Url &baseurl, const zypp::filesystem::Pathname &attachPoint) override;
    expected<void> ejectDevice(Device &dev) override;
    bool canHandle(const zypp::Url &url) override;
    std::vector< std::weak_ptr<DeviceHandler> > activeHandlers () override;

  private:
    std::vector< std::shared_ptr<Device> > _knownDevices;

    // The device key is a pair of major,minor kernel device numbers
    using DevKey = std::pair<int,int>;
    std::map< DevKey, std::shared_ptr<Device> > _sysDevs;
    bool _detectedSysDevs = false; // < did we already execute the auto detection
  };

  class CDRomDeviceHandler : public DeviceHandler
  {
    public:
      CDRomDeviceHandler( Device &dev, const zypp::Url &baseUrl, DeviceManager &parent );

      // DeviceHandler interface
      bool isIdle() const override;
  };

}
#endif
