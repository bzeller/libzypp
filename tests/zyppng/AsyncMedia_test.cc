#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-core/zyppng/pipelines/Redo>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/zyppng/pipelines/Wait>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Regex.h>
#include <zypp-core/zyppng/thread/Wakeup>
#include <zypp-core/zyppng/base/SocketNotifier>

#include <zypp/zyppng/media/DeviceManager>
#include <zypp/zyppng/media/DeviceHandler>
#include <zypp/zyppng/media/Device>

#include <zypp/MediaSetAccess.h>

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

#include <zypp-core/Url.h>
#include <iostream>
#include <algorithm>
#include <fstream>

void writeError ( const std::string_view str, std::exception_ptr excp )
{
  try {
    std::rethrow_exception( excp );
  } catch ( const zypp::Exception &e ) {
    std::cout << str << " failed with error: " << e.asString() << std::endl;
  } catch ( const std::exception &e ) {
    std::cout << str << " failed with error: " << e.what() << std::endl;
  } catch ( ...  ) {
    auto e = std::current_exception();
    std::cout << str << " failed with error: unknown exception" << std::endl;
  }
}

using namespace zyppng::operators;

using StringList = std::vector<std::string>;


zyppng::expected<StringList> findAllMatches ( const zypp::Pathname &path, const std::string &regex )
{
  using Ret = zyppng::expected<StringList>;
  try {
    std::cout << "Running in thread: " << std::this_thread::get_id() << std::endl;
    std::fstream data( path.c_str(), std::fstream::in );
    if ( !data.is_open() ) {
      std::cout << "Failed to open stuff" << std::endl;
      return  Ret::error( std::make_exception_ptr( zypp::Exception("Failed to open the provided file:"+path.asString()+")")));
    }
    zypp::str::regex rx( regex );
    StringList res;
    std::string line;
    while (std::getline( data, line )) {
      if ( !line.size() )
        continue;

      if ( rx.matches( line ) ) {
        res.push_back( line );
      }
    }
    std::cout << "Fin in thread: " << std::this_thread::get_id() << std::endl;
    return Ret::success(res);
  }  catch ( ... ) {
    return Ret::error( std::current_exception() );
  }
}


zyppng::expected<StringList> readAllLines ( const zypp::Pathname &path )
{
  using Ret = zyppng::expected<StringList>;
  std::fstream data( path.c_str(), std::fstream::in );
  if ( !data.is_open() ) {
    std::cout << "Failed to open stuff" << std::endl;
    return  Ret::error( std::make_exception_ptr( zypp::Exception("Failed to open the provided file:"+path.asString()+")")));

  }
  StringList res;
  std::string line;
  while (std::getline( data, line )) {
    if ( !line.size() )
      continue;

    std::cout << "Got line: " << line << std::endl;
    res.push_back( line );
  }
  return Ret::success(res);
}

class FileMatcher : public zyppng::AsyncOp<zyppng::expected<StringList>>
{
public:

  FileMatcher( const std::string &regex, boost::asio::thread_pool &pool ) : _regex(regex), _pool(pool) { }

  void operator()( zyppng::PathRef &&pathRef ) {

    if ( _notify ) {
      // this is already running EEEEK , this can never happen we bail out ugly:
      throw zypp::Exception("Double called a running FileMatcher");
    }

    std::cout << "Received file: " << pathRef.path().asString() << std::endl;
    _notify = _wk.makeNotifier( true );
    _notifyConn = connect( *_notify, &zyppng::SocketNotifier::sigActivated, *this, &FileMatcher::threadReady );
    _pathRef = std::move( pathRef );
    boost::asio::post( _pool, [this, pR = _pathRef.path()] () mutable {
      _res = findAllMatches( pR, _regex );
      _wk.notify();
    });
  }

private:

  void threadReady ( const zyppng::SocketNotifier &, int ) {
    _notifyConn.disconnect();
    _notify.reset();
    _pathRef.reset();
    _wk.ack();
    setReady( std::move( *_res ) );
  }

  std::string _regex;
  boost::asio::thread_pool &_pool;
  zyppng::PathRef _pathRef; // need to keep the handler around
  zyppng::Wakeup _wk;
  zyppng::connection _notifyConn;
  zyppng::SocketNotifier::Ptr _notify;
  std::optional<value_type> _res;
};

auto findMatchesInFile ( const std::string &regex, boost::asio::thread_pool &pool ){
  return [ regex, &pool ]( auto &&arg ) -> zyppng::AsyncOpRef<zyppng::expected<StringList>> {
    using Arg = decltype (arg);
    auto op = std::make_shared<FileMatcher>(regex, pool);
    (*op)( std::forward<Arg>(arg) );
    return op;
  };
}

zyppng::AsyncOpRef<zyppng::expected<zyppng::DeviceHandler::Ptr>> findHandlerFor ( zyppng::DeviceManager::Ptr devMgr, const zypp::Url &url1 )
{

  auto devs = devMgr->findDevicesFor( url1 );
  if ( !devs.size() ) {
    return zyppng::makeReadyResult( zyppng::expected<zyppng::DeviceHandler::Ptr>::error( std::make_exception_ptr( zypp::Exception("No handler found")) ) );
  } else {
    return zyppng::makeReadyResult( devMgr->attachTo( devs.front(),  url1, "/tmp/test" ) );
  }
#if 0
  return devMgr->findDevicesFor( url1 )
    | []( auto &&vec ){
        return std::make_pair( std::make_shared<int>(0), std::move(vec) );
           } | zyppng::redo_while( [&devMgr, &url1 ]( std::pair<std::shared_ptr<int>, std::vector<zyppng::Device::Ptr>> &&pair ) -> zyppng::AsyncOpRef<zyppng::expected<zyppng::DeviceHandler::Ptr>>
      {
        using Ret = zyppng::expected<zyppng::DeviceHandler::Ptr>;
        int myDev = *pair.first;
        (*pair.first)++; // increase the counter here

        if ( myDev >=  pair.second.size() )
          return zyppng::makeReadyResult(Ret::error( std::make_exception_ptr(std::out_of_range("No device available to handle the request.")) ));

        return devMgr->attachTo( pair.second[myDev], url1, "/tmp/test" )
               | mbind( [ dev = pair.second[myDev] ]( auto &&handler ) -> zyppng::AsyncOpRef<zyppng::expected<std::shared_ptr<zyppng::DeviceHandler>>> {
                   using Ret = zyppng::expected<std::shared_ptr<zyppng::DeviceHandler>>;
                   if ( !handler ) {
                     return zyppng::makeReadyResult( Ret::error(ZYPP_EXCPT_PTR(zypp::Exception("Invalid handler"))));
                   }
                   std::cout << "Okay, we got a handler, lets see" << std::endl;
                   std::cout << "Device reports attached: " << dev->isAttached() << std::endl;

                   return handler->doesFileExist( "/history/list" )
                          | mbind( [ hdl = handler ]( bool && ) -> zyppng::expected<std::shared_ptr<zyppng::DeviceHandler>> {
                              std::cout << "The file exists, lets use this handler" << std::endl;
                              return zyppng::expected<std::shared_ptr<zyppng::DeviceHandler>>::success(hdl);
                            });
                 });
      }, []( auto &&res ) -> bool {
        if ( res.is_valid() ) {
          return false; //stop we got one
        }
        try {
          std::rethrow_exception( res.error() );
        }  catch ( const std::out_of_range &e) {
          return false; //stop we iterated over everything
        } catch (...) {
          ;
        }
        return true; // continue
      });
#endif
}


zyppng::AsyncOpRef<zyppng::expected<StringList>> getAvailableDates( zyppng::DeviceHandler::Ptr &&handler )
{
  return handler->provideFile( zypp::OnMediaLocation("/history/list") )
         | mbind( []( zyppng::PathRef &&pathRef ) -> zyppng::expected<StringList> {
             return readAllLines( pathRef.path() );
           });
}

auto requestFilesForDateCB ( zyppng::DeviceHandler::Ptr &&hdl, boost::asio::thread_pool &pool ) {
  return [ hdl, &pool ]( std::string && date ) -> zyppng::AsyncOpRef<zyppng::expected<std::pair<std::string, StringList>>>
  {
    std::cout << "Requesting file for date: " << date << std::endl;
    std::string url = "/history/"+zypp::str::trim(date)+"/rpm.list";
    return hdl->provideFile( zypp::OnMediaLocation(url) )
      | mbind( findMatchesInFile(".*debuginfo.*", pool) )
      | mbind( [ date ]( auto &&list ){
        using Res = zyppng::expected<std::pair<std::string, StringList>>;
        return Res::success( std::make_pair( date, std::move(list)) );
      });
  };
}

int doWithAsyncMedia ()
{
  std::cout << "Providing file with async API" << std::endl;

  auto ev = zyppng::EventLoop::create();
  auto devMgr = zyppng::DeviceManager::create();


  boost::asio::thread_pool pool;

  zypp::Url url1("http://download.opensuse.org");
  auto h = findHandlerFor( devMgr, url1 )
           | mbind( [ &pool ]( zyppng::DeviceHandler::Ptr &&handler ) {
               return getAvailableDates( zyppng::DeviceHandler::Ptr(handler) )
                      | mbind( [ handler, &pool ]( auto &&in ){
                        return zyppng::make_expected_success(
                          zyppng::transform(
                            std::move(in),
                            requestFilesForDateCB(zyppng::DeviceHandler::Ptr(handler) , pool))
                          );
                        })
                      | mbind( []( auto &&expRes ) {
                          return std::move(expRes)
                                  | waitFor< zyppng::expected<std::pair<std::string, StringList>> >()
                                  | []( auto &&list ){
                                    std::for_each( list.begin(), list.end(), []( auto &res){
                                      if ( res.is_valid() ) {
                                        std::cout << "Matches: " << std::endl;
                                        for ( const auto &match : res.get().second )
                                          std::cout << res.get().first << ":" << match << std::endl;
                                      } else {
                                        writeError( "Failed", res.error() );
                                      }
                                    });
                                  return zyppng::expected<void>::success();
                                  };
                        });
             });

  int retCode = 0;
  const auto &handleReady = [&]( zyppng::expected<void> &&res ) {
    if ( !res )
      retCode = 1;
    ev->quit();
  };

  if ( !h->isReady() ) {
    h->onReady( handleReady );
    ev->run();
  } else {
    handleReady( std::move(h->get()) );
  }

  pool.join();

  return retCode;
}



int doWithOldMedia ()
{
  std::cout << "Providing file with SYNC API" << std::endl;
  try {
    zypp::MediaSetAccess acc( zypp::Url("http://download.opensuse.org"), "/tmp/test" );
    auto path = acc.provideFile( zypp::OnMediaLocation("/history/list") );
    auto allDates = readAllLines( path );
    if ( !allDates ) {
      std::cout << "Reading all dates failed" << std::endl;
      return 1;
    }

    std::vector<StringList> allmatches;

    for ( const auto &date : allDates.get() ) {
      auto dataFile = acc.provideFile( zypp::OnMediaLocation( "/history/"+zypp::str::trim(date)+"/rpm.list" ) );
      auto matches = findAllMatches( dataFile, ".*debuginfo.*" );
      if ( !matches ) {
        writeError( "Failed to find matches for date " + date, matches.error() );
        continue;
      }
      allmatches.push_back( matches.get() );
    }

    for ( const auto &matchList : allmatches ) {
      for ( const auto &match : matchList ) {
        std::cout << "Match: " << match << std::endl;
      }
    }
    return 0;
  }  catch ( ... ) {
    writeError("Exception", std::current_exception() );
    return 1;
  }
}


int main( int argc, char * argv[] )
{
  if ( argc > 1 && std::string_view(argv[1]) == "old" )
    return doWithOldMedia();
  else
    return doWithAsyncMedia();
}
