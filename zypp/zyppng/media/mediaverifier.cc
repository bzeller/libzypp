/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "mediaverifier.h"

namespace zyppng {

  AsyncOpRef<expected<void> > NoVerifier::isDesiredMedia( const DeviceHandler & ) const
  {
    return makeReadyResult( expected<void>::success () );
  }

}
