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
*/
#ifndef ZYPP_ZYPPNG_PIPELINES_IO_H
#define ZYPP_ZYPPNG_PIPELINES_IO_H

#include "zypp-core/zyppng/pipelines/expected.h"
#include <iostream>
#include <zypp-core/zyppng/io/IODevice>

namespace zyppng {

  class IoException : public zypp::Exception
  {
    public:
      IoException();
      IoException( const std::string & msg_r );
      IoException( const std::string & msg_r, const std::string & hist_r );
      ~IoException() throw() override;
  };

  namespace io {
    expected<ByteArray> readline( std::istream & stream );
    AsyncOpRef<expected<ByteArray>> readline( IODeviceRef device );

    expected<ByteArray> read( std::istream & stream, int64_t bytes );
    AsyncOpRef<expected<ByteArray>> read( IODeviceRef device, int64_t bytes );

    expected<ByteArray> read_until( std::istream & stream, const char delim );
    AsyncOpRef<expected<ByteArray>> read_until( IODeviceRef device, const char delim );
  }
}

#endif

