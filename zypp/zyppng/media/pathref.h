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

#ifndef ZYPPNG_MEDIA_PATHREF_H_INCLUDED
#define ZYPPNG_MEDIA_PATHREF_H_INCLUDED

#include <zypp/Pathname.h>
#include <zypp-core/zyppng/base/Signals>
#include <memory>

namespace zyppng {

  class PathRefPrivate;
  class DeviceHandler;

  /**
   * \class PathRef
   * A PathRef object is a reference counted ownership of a file provided by
   * a \ref DeviceHandler, keeping either the parent DeviceHandler alive if
   * the file was provided into the device's mountpoint or alternatively cleaning
   * up the file from a local temporary cache as soon as the last reference to it is
   * released.
   * It is generally advisable to release a PathRef instance asap, to make sure ressources
   * can be released and Device instances are free to be ejected.
   */
  class PathRef
  {
  public:

    /*!
     * Creates a PathRef that is empty
     */
    PathRef();

    /**
     * Creates a new PathRef Object, keeping a reference to the DeviceHandler alive
     * as long as the PathRef is in existance. This will block the Device from being
     * automatially ejected. Make sure to release PathRef objects asap.
     */
    PathRef( std::shared_ptr<DeviceHandler> &&ref, zypp::Pathname path );

#if 0
    /**
     * Construct a new Path Ref object pointing to a file that was copied into a local
     * cache, outside of a Device's mountpoint. These PathRef instances are used if
     * local mirroring of a DeviceHandler is enabled.
     */
    PathRef( zypp::Pathname path );
#endif

    PathRef( const PathRef &other ) = default;
    PathRef( PathRef &&other ) = default;

    ~PathRef();

    const zypp::Pathname &path() const;
    const std::shared_ptr<DeviceHandler> &handler() const;
    SignalProxy<void()> sigReleased ();

    PathRef &operator=( const PathRef &other ) = default;
    PathRef &operator=( PathRef &&other ) = default;

  private:
    std::shared_ptr<PathRefPrivate> d_ptr;
  };


}



#endif // ZYPPNG_MEDIA_PATHREF_H_INCLUDED
