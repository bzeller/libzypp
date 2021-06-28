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
#ifndef ZYPPNG_MEDIA_DEVICEHANDLER_H_INCLUDED
#define ZYPPNG_MEDIA_DEVICEHANDLER_H_INCLUDED

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/Pathname.h>
#include <zypp-core/AutoDispose.h>
#include <zypp-core/ManagedFile.h>
#include <zypp-core/Url.h>
#include <zypp/PathInfo.h>
#include <zypp/OnMediaLocation.h>
#include <zypp/zyppng/media/PathRef>

namespace zyppng {

  class Device;
  class DeviceManager;
  class DeviceHandler;

  template <typename T, typename DT>
  class AsyncDeviceOp;

  using DeviceAttachPointRef = zypp::AutoDispose<const zypp::Pathname>;
  #if 0
  class DeviceAttachPointRef : public zypp::AutoDispose<const zypp::Pathname>
  {
    using AutoDispose::AutoDispose;
  };
  #endif


  /*!
   * A \a DeviceHandler is the representation of a mounted \ref Device that
   * provides the means to get files from the device. Releasing the \a DeviceHandler
   * will clean up the ressources it is using as long as there is no other reference
   * to it.
   * Each asynchronous operation returned will keep a reference to the \a DeviceHandler
   * as long as it is running. Each \ref PathRef also keeps a reference to the \a DeviceHandler
   * to make sure the file is accessible.
   *
   * @TODO enable local mirroring ( copying files out of the mountpoint )
   *
   */
  class DeviceHandler : public zyppng::Base
  {

  public:

    using Ptr = std::shared_ptr<DeviceHandler>;

    DeviceHandler( Device &dev, const zypp::Url &baseUrl, const zypp::Pathname& localRoot, DeviceManager &parent );

    virtual void setAttachPoint ( DeviceAttachPointRef &&ap );

    virtual ~DeviceHandler();

    DeviceManager & deviceManager();
    const DeviceManager & deviceManager() const;

    /*!
     * Removes the request with ID \a rId from the internal list of running requests
     */
    void opFinished ( uint rId );


    /*!
     * Unique device handler ID, must be unique for the device it is using
     */
    std::string id () const;

    /*!
     * The current attach point of the device, this CAN be shared between multiple DeviceHandlers
     */
    const DeviceAttachPointRef & attachPoint () const;

    /*!
     * Files provided will be available at 'localPath(filename)'.
     *
     * Returns empty pathname if E_bad_attachpoint
     */
    zypp::Pathname localPath( const zypp::Pathname & pathname ) const;

    const zypp::Url &url() const;
    bool doesLocalMirror() const;
    void setDoesLocalMirror(bool newDoesLocalMirror);
    zypp::Pathname localRoot() const;

    /*!
     * Remove filename below localRoot IFF handler mirrors files
     * to the local filesystem. Never remove anything from media.
     *
     * \returns MediaException on error, otherwise void
     *
     */
    virtual expected<void> releaseFile( const zypp::Pathname & filename ) { return releasePath( filename ); }

    /*!
     * Remove directory tree below localRoot IFF handler mirrors files
     * to the local filesystem. Never remove anything from media.
     *
     * \returns MediaException on error, otherwise void
     *
     */
    virtual expected<void> releaseDir( const zypp::Pathname & dirname ) { return releasePath( dirname ); }

    /*!
     * Remove pathname below localRoot IFF handler mirrors files
     * to the local filesystem. Never remove anything from media.
     *
     * If pathname denotes a directory it is recursively removed.
     * If pathname is empty or '/' everything below the localRoot
     * is recursively removed.
     * If pathname denotes a file it is unlinked.
     *
     * \returns MediaException on error, otherwise void
     */
    virtual expected<void> releasePath( zypp::Pathname pathname );

    /*!
     * Provide a content list of directory on media via returned AsyncOp.
     * If dots is false entries starting with '.' are not reported.
     *
     * Return \ref MediaException if media does not support retrieval of
     * directory content.
     *
     * Default implementation provided, that returns the content of a
     * directory at 'localRoot + dirnname' retrieved via 'readdir'.
     *
     * Asserted that media is attached
     *
     * \returns MediaException on errors otherwise the content list
     *
     **/
    AsyncOpRef<expected<std::list<std::string>>> dirInfo( const zypp::Pathname & dirname, bool dots = true );


    /*!
     * Basically the same as \ref dirInfo. The content list is returned as
     * filesystem::DirContent, which includes name and filetype of each directory
     * entry. Retrieving the filetype usg. requires an additional ::stat call for
     * each entry, thus it's more expensive than a simple readdir.
     *
     * Asserted that media is attached
     *
     * \returns MediaException on errors , otherwise the DirContent
     *
     **/
    AsyncOpRef<expected<zypp::filesystem::DirContent>> dirInfoExt( const zypp::Pathname &dirname, bool dots = true );


    /*!
     * Provide directory content (not recursive!) below attach point.
     *
     * Return \ref MediaException if media does not support retrieval of
     * directory content.
     *
     * Default implementation provided, that returns whether a directory
     * is located at 'localRoot + dirname'.
     *
     * Asserted that media is attached.
     *
     * \returns MediaException on errors or a reference to the provided directory
     *
     **/
     AsyncOpRef<expected<PathRef>> provideDir( const zypp::Pathname & dirname, bool recurse_r );

    /*!
     * Call concrete handler to provide file below attach point.
     *
     * Default implementation provided, that returns whether a file
     * is located at 'localRoot + filename'.
     *
     * Asserted that media is attached.
     *
     * \returns MediaException, otherwise a reference to the provided file
     *
     **/
     AsyncOpRef< expected<PathRef> > provideFile( const zypp::OnMediaLocation &file );

    /*!
     * check if a file exists
     *
     * Asserted that filename is a file and not a dir.
     *
     * \throws returns a MediaException on errors otherwise evaluates to a boolean for the detection result
     *
     **/
     AsyncOpRef<expected<bool>> doesFileExist( const zypp::Pathname & filename );

     template< typename Result, typename Handler >
     friend class AsyncDeviceOp;
     friend class PathRefPrivate;

  protected:

    /*!
     * Provide a content list of directory on media via returned AsyncOpRef.
     * If dots is false entries starting with '.' are not reported.
     *
     * Return \ref MediaException if media does not support retrieval of
     * directory content.
     *
     * Default implementation provided, that returns the content of a
     * directory at 'localRoot + dirnname' retrieved via 'readdir'.
     *
     * Asserted that media is attached
     *
     * \returns MediaException on errors otherwise the content list
     *
     **/
    virtual AsyncOpRef<expected<std::list<std::string>>> getDirInfo( const zypp::Pathname & dirname, bool dots = true );


    /*!
     * Basically the same as getDirInfo above. The content list is returned as
     * filesystem::DirContent, which includes name and filetype of each directory
     * entry. Retrieving the filetype usg. requires an additional ::stat call for
     * each entry, thus it's more expensive than a simple readdir.
     *
     * Asserted that media is attached
     *
     * \returns MediaException on errors , otherwise the DirContent
     *
     **/
    virtual AsyncOpRef<expected<zypp::filesystem::DirContent>> getDirInfoExt( const zypp::Pathname &dirname, bool dots = true );

    /*!
     *  Retrieve and if available scan dirname/directory.yast.
     */
    AsyncOpRef<expected<zypp::filesystem::DirContent>> getDirInfoYast( const zypp::Pathname & dirname, bool dots = true );


    /*!
     * Provide directory content (not recursive!) below attach point.
     *
     * Return \ref MediaException if media does not support retrieval of
     * directory content.
     *
     * Default implementation provided, that returns whether a directory
     * is located at 'localRoot + dirname'.
     *
     * Asserted that media is attached.
     *
     * \returns MediaException on errors or a reference to the provided directory
     *
     **/
    virtual AsyncOpRef<expected<PathRef>> getDir( const zypp::Pathname & dirname, bool recurse_r );

    /*!
     * Call concrete handler to provide file below attach point.
     *
     * Default implementation provided, that returns whether a file
     * is located at 'localRoot + filename'.
     *
     * Asserted that media is attached.
     *
     * \returns MediaException, otherwise a reference to the provided file
     *
     **/
    virtual AsyncOpRef< expected<PathRef> > getFile( const zypp::OnMediaLocation &file );

    /*!
     * check if a file exists
     *
     * Asserted that filename is a file and not a dir.
     *
     * \throws returns a MediaException on errors otherwise evaluates to a boolean for the detection result
     *
     **/
    virtual AsyncOpRef<expected<bool>> checkDoesFileExist( const zypp::Pathname & filename );

    /*!
     * Emitted in case all running requests should immediately stop
     */
    SignalProxy<void()> sigShutdown ();

    /*!
     * Initiates a immediate shutdown of the handler, this should only be called
     * by the device before its ejected/unmounted to force invalidation of all running jobs and existing
     * refrences. If a handler is not needed anymore in normal code, just release all references to it.
     */
    virtual void immediateShutdown ();

  private:

    DeviceManager &_myManager;

    connection _deviceShutdownConn;

    /*!
     * The relative root directory of the data on the media.
     * See also localRoot() and urlpath_below_attachpoint_r
     * constructor argument.
     *
     * This basically emulates mounting a subdir of the targeted device to the local
     * attachpoint, for example dvd://path/to/repo, mounting a DVD always mounts the
     * root of the device to the attachpoint, the /path/to/repo will however be prepended
     * to all requests by the handler, making it look like we have the subdir mounted directly.
     */
    zypp::Pathname        _relativeRoot;

    /*!
     * The reference to the attach point, this is the local physical
     * location, in some cases its a actual mountpoint in others it might
     * be a temporary directory.
     */
    DeviceAttachPointRef  _attachPoint;

    /*!
     * The initial Url that was passed to the media manager in order to get this handler
     */
    zypp::Url _url;

    /*!
     * If the Handler mirrors file locally this should be set to true
     */
    bool _doesLocalMirror = false;

    /*!
     * The device we are currently using
     */
    std::shared_ptr<Device> _myDevice;

    /*!
     * The mount ID we got from the device
     */
    uint _myMountId = 0;

    Signal<void ()> _sigShutdown;

  };

  using DeviceHandlerRef = DeviceHandler::Ptr;
}




#endif
