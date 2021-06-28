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
#ifndef ZYPPNG_MEDIA_MEDIASETHANDLER_H_INCLUDED
#define ZYPPNG_MEDIA_MEDIASETHANDLER_H_INCLUDED

#include <zypp-core/Pathname.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp/zyppng/media/DeviceHandler>
#include <zypp/zyppng/media/PathRef>

#include <zypp-core/Url.h>
#include <zypp/PathInfo.h>
#include <zypp/OnMediaLocation.h>

namespace zyppng
{
  class Context;
  class MediaSetHandlerPrivate;


/**
 * Handler class that implements all secrets of using a multi volume datasource. E.g. a set of DVDs or ISOs.
 * The first volume contains a file called "media.1/media" to describe the full dataset. The handler automatically
 * will ask the user to exchange volumes if necessary. Each volume in the set has its own identifier called "medianr".
 * The description file specified wich files are found on which volume. The MediaSetHandler will try to limit the media
 * change requests to the absolute minimum by sorting the requests according to the medianr.
 *
 * A MediaSetHandler will always wait to start the requested operations until the code returns control to the EventLoop,
 * this is required to give the AsycOps a chance to actually be processed before a media change is initiated.
 *
 * Instead of using the DeviceHandler directly, this is the go to class to use for
 * all file downloading / providing needs.
 **/
  class MediaSetHandler : public zyppng::Base
  {
    ZYPP_DECLARE_PRIVATE(MediaSetHandler)

  public:
    using Ptr =  std::shared_ptr<MediaSetHandler>;

    /**
         * Url used.
         **/
    zypp::Url url() const;
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
     AsyncOpRef<expected<PathRef> > provideFile( const zypp::OnMediaLocation &file );

    /*!
     * check if a file exists
     *
     * Asserted that filename is a file and not a dir.
     *
     * \throws returns a MediaException on errors otherwise evaluates to a boolean for the detection result
     *
     **/
     AsyncOpRef<expected<bool>> doesFileExist( const zypp::Pathname & filename );
  };
}

#endif
