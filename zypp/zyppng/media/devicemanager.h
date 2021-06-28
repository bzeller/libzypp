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
#ifndef ZYPPNG_MEDIA_DEVICEMANAGER_H_INCLUDED
#define ZYPPNG_MEDIA_DEVICEMANAGER_H_INCLUDED

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/Url.h>
#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp/media/Mount.h>
#include <functional>

namespace zyppng {

  // The device handler contains:
  // - A ref to the Devicespec
  // - A mountpoint
  // - Handler specific settings
  class DeviceManager;
  class DeviceHandler;
  class Device;

  using AsyncMountOp = AsyncOp<expected<std::shared_ptr<DeviceHandler>>>;

  /**
   * \class DeviceType
   *
   * A DeviceType is basically the factory type for a specific device driver. It provides the basic
   * funtionalities to discover and attach existing devices for a specific BaseUrl. It can be used
   * to implement additional device drivers via plugins.
   * Every DeviceType implementation has to cache all Device instances that were returned to the
   * API as long as they are in use, for device types where the detection is expensive it is advised
   * to keep the Devices around as long as they are valid in the system.
   */
  class DeviceType : public zyppng::Base
  {
  public:
    virtual ~DeviceType();
    bool isVirtual () const;
    const std::string &type () const;

    DeviceManager & deviceManager();
    const DeviceManager & deviceManager() const;

    /*!
     * Detect and return all devices that are suitable to satisfy requests to the given \a url.
     */
    virtual std::vector< std::shared_ptr<Device> > detectDevices ( const zypp::Url &url, const std::vector<std::string> &filters = {} ) = 0;

    /*!
     * Returns weak pointers to all currently active handlers that are associated to a device specific to this handler.
     */
    virtual std::vector< std::weak_ptr<DeviceHandler> > activeHandlers () = 0;

    /*!
     * Attach a given \ref Device \a dev to a local mountpoint. If this is a real mountpoint or just a plain directory where the driver
     * mirrors the requested files is completely up the implementation. If successful returns a new \ref DeviceHandler instance that
     * can be used to provide files from the medium. The Attachpoint is released together with the last reference to the \ref DeviceHandler.
     *
     * \note the DeviceHandler will take ownership of the attachpoint and remove it after the last reference is released
     */
    virtual expected< std::shared_ptr<DeviceHandler> > attachDevice ( std::shared_ptr<Device> dev, const zypp::Url &baseurl, const zypp::Pathname &preferredAttachPoint ) = 0;
    virtual expected<void> ejectDevice ( Device &dev ) = 0;
    virtual bool canHandle ( const zypp::Url &url ) = 0;

  protected:
    DeviceType ( DeviceManager &devMgr, const std::string &typeName, bool isVirtual = false );
    expected<void> prepareEject ( Device &dev );

    /*!
     * Small helper callback for all DeviceHandler that use physical
     * mountpoints. This simply executes "umount <attachpoint>"
     */
    static void umountHelper ( const zypp::Pathname &mountPoint );

  private:
    std::string _type;
    bool _isVirtual = false;
    DeviceManager &_devMgr;
  };

  class DeviceManagerPrivate;
  class DeviceManager : public zyppng::Base
  {
  public:

    using Ptr = std::shared_ptr<DeviceManager>;

    static std::shared_ptr<DeviceManager> create ();

    /*!
     * Tries to find a device for the given URL, the device can be attached already
     * in case it is shared.
     */
    std::vector< std::shared_ptr<Device> > findDevicesFor ( const zypp::Url &url, const std::vector<std::string> &filters = {} );

    /*!
     * Attaches a given device to a mount/attach point. Devices can be attached multiple
     * times. Its up for the caller to decide if that makes sense or not.
     *
     * \returns A expected thats either DeviceHandlerPtr or a MediaException
     */
    expected<std::shared_ptr<DeviceHandler>> attachTo ( std::shared_ptr<Device> dev, const zypp::Url &baseurl, const zypp::Pathname &preferredAttachPoint );

    /*!
     * Registers a new device type. A \ref DeviceType is basically a factory for device
     * drivers.
     */
    void registerDeviceType ( std::shared_ptr<DeviceType> type );


    /*!
     * Looks up and returns a registered DeviceType for a device identifier,
     * or nullptr if nothing was found
     */
    std::shared_ptr<DeviceType> deviceType ( const std::string &devType );


    /*!
     * Checks if the given mountpoint is free, e.g. no other handler is using it and no device is currently
     * mounted in that location.
     */
    bool isUseableAttachPoint ( const zypp::Pathname &path ) const;

    /*!
     * Tries to find the mount entry for a given mountpoint, returns a MediaException otherwise
     */
    expected<zypp::media::MountEntry> findMount ( const zypp::Pathname &path);

    /*!
     * Tries to find all mountpoints for a given device in the mtab
     */
    std::vector<zypp::media::MountEntry> findMounts ( const Device &dev );

  private:
    DeviceManager();
    ZYPP_DECLARE_PRIVATE( DeviceManager );
  };


}


#endif
