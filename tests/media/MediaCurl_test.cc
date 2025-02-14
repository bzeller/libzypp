#include "WebServer.h"

#include <zypp/media/MediaManager.h>
#include <zypp-core/Url.h>
#include <zypp/Digest.h>

#include <iostream>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

namespace bdata = boost::unit_test::data;

// Globals are HORRIBLY broken here, do not use any static globals they might not be initialized at the point
// of calling the initializer of the MirrorSet vector, so we are manually initializing the Byte and KByte Units here
const auto makeBytes( zypp::ByteCount::SizeType size ) {
  return zypp::ByteCount( size, zypp::ByteCount::Unit( 1LL, "B", 0 ) );
};

const auto makeKBytes( zypp::ByteCount::SizeType size ) {
  return zypp::ByteCount( size, zypp::ByteCount::Unit( 1024LL, "KiB", 1 ) );
};

bool withSSL[]{ true, false};
std::vector<std::string> backend = {"curl2"}; //{"curl", "curl2", "multicurl"};

BOOST_DATA_TEST_CASE(base_provide_zck, bdata::make( withSSL ) * bdata::make( backend ), withSSL, backend )
{

  int primaryRequests = 0;

  zypp::Pathname testRoot = zypp::Pathname(TESTS_SRC_DIR)/"zyppng/data/downloader";
  WebServer web( testRoot.c_str(), 10001, withSSL );
  BOOST_REQUIRE( web.start() );

  web.addRequestHandler("primary", [ & ]( WebServer::Request &req ){
    primaryRequests++;
    req.rout << "Location: /primary.xml.zck\r\n\r\n";
    return;
  });

  zypp::media::MediaManager   mm;
  zypp::media::MediaAccessId  id;

  zypp::Url mediaUrl = web.url();
  mediaUrl.setQueryParam( "mediahandler", backend );

  if( withSSL ) {
    mediaUrl.setQueryParam("ssl_capath", web.caPath().asString() );
  }

  BOOST_CHECK_NO_THROW( id = mm.open( mediaUrl ) );
  BOOST_CHECK_NO_THROW( mm.attach(id) );

  zypp::OnMediaLocation loc("/handler/primary");
  loc.setDeltafile( testRoot/"primary-deltatemplate.xml.zck" );
  loc.setDownloadSize( makeBytes(274638) );
  loc.setHeaderSize( makeBytes(11717) );
  loc.setHeaderChecksum( zypp::CheckSum( zypp::Digest::sha256(), "90a1a1b99ba3b6c8ae9f14b0c8b8c43141c69ec3388bfa3b9915fbeea03926b7") );

  BOOST_CHECK_NO_THROW( mm.provideFile( id, loc ) );

#ifdef ENABLE_ZCHUNK_COMPRESSION
  if ( backend == "curl2" )
    BOOST_REQUIRE_EQUAL( primaryRequests, 2 ); // header + chunks
  else
    BOOST_REQUIRE_EQUAL( primaryRequests, 1 );
#else
    BOOST_REQUIRE_EQUAL( primaryRequests, 1 );
#endif

}
