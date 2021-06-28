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

#ifndef ZYPP_ZYPPNG_MEDIA_MEDIAVERIFIER_H_INCLUDED
#define ZYPP_ZYPPNG_MEDIA_MEDIAVERIFIER_H_INCLUDED

#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp-core/zyppng/pipelines/Expected>
#include <string>

namespace zyppng {

  class DeviceHandler;

  /**
   * Interface to implement a media verifier.
   */
  class MediaVerifierBase //: private zypp::NonCopyable
  {
  public:
    MediaVerifierBase()
    {}

    virtual ~MediaVerifierBase()
    {}

    /**
     * Returns a string with some info about the verifier.
     * By default, the type info name is returned.
     */
    virtual std::string info() const;

    /*
    ** Check if the specified attached media contains
    ** the desired media (e.g. SLES15 DVD1).
    */
    virtual AsyncOpRef<expected<void>> isDesiredMedia( const DeviceHandler &ref ) const = 0;
  };

  using MediaVerifierRef = std::shared_ptr<MediaVerifierBase>;

  class NoVerifier :public MediaVerifierBase
  {
    // MediaVerifierBase interface
  public:
    static MediaVerifierRef create() {
      return std::make_shared<NoVerifier>();
    }
    AsyncOpRef<expected<void> > isDesiredMedia(const DeviceHandler &ref) const override;
  };

}



#endif // ZYPP_ZYPPNG_MEDIA_MEDIAVERIFYER_H_INCLUDED
