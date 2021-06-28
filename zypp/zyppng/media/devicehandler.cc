/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "devicehandler.h"

#include <zypp/zyppng/media/AsyncDeviceOp>
#include <zypp/zyppng/media/Device>
#include <zypp/zyppng/media/DeviceManager>
#include <zypp/PathInfo.h>
#include <zypp/media/MediaException.h>
#include <zypp-core/zyppng/pipelines/AsyncResult>

#include <zypp/base/LogControl.h>
#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::DeviceHandler"

#include <iostream>
#include <fstream>
#include <sstream>

namespace zyppng {

  const zypp::Url &DeviceHandler::url() const
  {
    return _url;
  }

  bool DeviceHandler::doesLocalMirror() const
  {
    return _doesLocalMirror;
  }

  void DeviceHandler::setDoesLocalMirror( bool newDoesLocalMirror )
  {
    _doesLocalMirror = newDoesLocalMirror;
  }

  DeviceHandler::DeviceHandler( Device &dev, const zypp::Url &baseUrl, const zypp::Pathname& localRoot, DeviceManager &parent  )
    : _myManager( parent )
    , _relativeRoot( localRoot )
    , _url( baseUrl )
    , _myDevice( dev.shared_this<Device>() )
  {
    _deviceShutdownConn = _myDevice->connect( &Device::sigAboutToEject, *this, &DeviceHandler::immediateShutdown );
  }

  void DeviceHandler::setAttachPoint( DeviceAttachPointRef &&ap )
  {
    // if ap is empty we unregister the current mount
    if ( ap->empty() ) {
      if ( _myMountId > 0 )
        _myDevice->delMount( _myMountId );
      _myMountId = 0;
    } else {
      // only register a new mount if we are not already registered
      if ( _myMountId == 0 ) {
        _myMountId = _myDevice->addMount( shared_this<DeviceHandler>() );
      }
    }
    _attachPoint = std::move(ap);
  }

  DeviceHandler::~DeviceHandler()
  {
    _myDevice->delMount( _myMountId );
  }

  DeviceManager &DeviceHandler::deviceManager()
  {
    return _myManager;
  }

  const DeviceManager &DeviceHandler::deviceManager() const
  {
    return _myManager;
  }

  const DeviceAttachPointRef &DeviceHandler::attachPoint() const
  {
    return _attachPoint;
  }

  zypp::Pathname DeviceHandler::localRoot() const
  {
    return _attachPoint.value() + _relativeRoot;
  }

  zypp::filesystem::Pathname zyppng::DeviceHandler::localPath(const zypp::filesystem::Pathname &pathname) const
  {
    zypp::Pathname _localRoot( _attachPoint.value() + _relativeRoot );
    if ( _localRoot.empty() )
      return _localRoot;
    return _localRoot + pathname.absolutename();
  }

  AsyncOpRef<expected<std::list<std::string>>> DeviceHandler::getDirInfo( const zypp::filesystem::Pathname &dirname, bool dots )
  {
    zypp::PathInfo info( localPath( dirname ) );
    if( ! info.isDir() ) {
      return makeReadyResult( expected<std::list<std::string>>::error( ZYPP_EXCPT_PTR( zypp::media::MediaNotADirException( url(), localPath( dirname )) ) ));
    }

    using namespace zyppng::operators;
    return getDirInfoYast( dirname, dots ) | [ dots, info, url = url() ]( expected<zypp::filesystem::DirContent> &&in ) -> expected<std::list<std::string>> {

      std::list<std::string> retlist;
      if ( in.is_valid() ) {
        // convert to std::list<std::string>
        const auto &content = in.get();
        for ( zypp::filesystem::DirContent::const_iterator it = content.begin(); it != content.end(); ++it )
          retlist.push_back( it->name );
      } else {
        // readdir
        int res = readdir( retlist, info.path(), dots );
        if ( res ) {
          zypp::media::MediaSystemException nexcpt( url, "readdir failed" );
          nexcpt.remember( in.error() );
          return expected<std::list<std::string>>::error( ZYPP_EXCPT_PTR(nexcpt) );
        }
      }
      return expected<std::list<std::string>>::success( retlist );
    };
  }

  AsyncOpRef<expected<zypp::filesystem::DirContent>> DeviceHandler::getDirInfoExt(const zypp::filesystem::Pathname &dirname, bool dots)
  {
    zypp::PathInfo info( localPath( dirname ) );
    if( ! info.isDir() ) {
      return makeReadyResult( expected<zypp::filesystem::DirContent>::error( ZYPP_EXCPT_PTR( zypp::media::MediaNotADirException( url(), localPath( dirname )) ) ));
    }

    using namespace zyppng::operators;
    return getDirInfoYast( dirname, dots ) | [ dots, info, url = url() ]( expected<zypp::filesystem::DirContent> &&in ) -> expected<zypp::filesystem::DirContent> {

      if ( in.is_valid() )
        return std::move(in);

      // readdir
      zypp::filesystem::DirContent retlist;
      int res = readdir( retlist, info.path(), dots );
      if ( res ) {
        zypp::media::MediaSystemException nexcpt( url, "readdir failed" );
        nexcpt.remember( in.error() );
        return expected<zypp::filesystem::DirContent>::error( ZYPP_EXCPT_PTR(nexcpt) );
      }
      return expected<zypp::filesystem::DirContent>::success( retlist );
    };
  }

  AsyncOpRef<expected<zypp::filesystem::DirContent> > DeviceHandler::getDirInfoYast( const zypp::filesystem::Pathname &dirname, bool dots )
  {
    using namespace zyppng::operators;
    auto dirFile = zypp::OnMediaLocation( dirname + "directory.yast" );

    return getFile( dirFile ) | mbind( [ dots ]( PathRef &&in ) -> expected<zypp::filesystem::DirContent> {

      auto handler = in.handler();
      const auto &dirFile = in.path();
      const auto &locPath = handler->localPath( dirFile );
      DBG << "provideFile(" << dirFile << "): " << "OK" << std::endl;

      // using directory.yast
      std::ifstream dir( locPath.asString().c_str() );
      if ( dir.fail() ) {
        ERR << "Unable to load '" << handler->localPath( dirFile ) << "'" << std::endl;
        return expected<zypp::filesystem::DirContent>::error( ZYPP_EXCPT_PTR( zypp::media::MediaSystemException( handler->url(),
          "Unable to load '" + locPath.asString() + "'")) );
      }

      zypp::filesystem::DirContent retlist;
      std::string line;
      while( getline( dir, line ) ) {
        if ( line.empty() ) continue;
        if ( line == "directory.yast" ) continue;

        // Newer directory.yast append '/' to directory names
        // Remaining entries are unspecified, although most probabely files.
        zypp::filesystem::FileType type = zypp::filesystem::FT_NOT_AVAIL;
        if ( *line.rbegin() == '/' ) {
          line.erase( line.end()-1 );
          type = zypp::filesystem::FT_DIR;
        }

        if ( dots ) {
          if ( line == "." || line == ".." ) continue;
        } else {
          if ( *line.begin() == '.' ) continue;
        }

        retlist.push_back( zypp::filesystem::DirEntry( line, type ) );
      }

      return expected<zypp::filesystem::DirContent>::success( retlist );
    });
  }

  AsyncOpRef< expected<PathRef> > DeviceHandler::getDir( const zypp::filesystem::Pathname &dirname, bool )
  {
    zypp::PathInfo info( localPath( dirname ) );
    if( info.isFile() ) {
      return makeReadyDeviceOp( shared_this<DeviceHandler>(), expected<PathRef>::success( PathRef( shared_this<DeviceHandler>(), info.path() )) );
    }

    if (info.isExist())
      return makeReadyResult( expected<PathRef>::error( ZYPP_EXCPT_PTR( zypp::media::MediaNotADirException( url(), localPath( dirname )) ) ) );
    else
      return makeReadyResult( expected<PathRef>::error( ZYPP_EXCPT_PTR( zypp::media::MediaFileNotFoundException( url(), dirname ) ) ) );
  }

  AsyncOpRef< expected<PathRef> > DeviceHandler::getFile( const zypp::OnMediaLocation &file )
  {
    zypp::PathInfo info( localPath( file.filename() ) );
    if( info.isFile() ) {
      return makeReadyDeviceOp( shared_this<DeviceHandler>(), expected<PathRef>::success( PathRef( shared_this<DeviceHandler>(), info.path() )) );
    }

    if (info.isExist())
      return makeReadyResult( expected<PathRef>::error( ZYPP_EXCPT_PTR( zypp::media::MediaNotAFileException( url(), localPath(file.filename())) ) ) );
    else
      return makeReadyResult( expected<PathRef>::error( ZYPP_EXCPT_PTR( zypp::media::MediaFileNotFoundException( url(), file.filename()) ) ) );
  }

  AsyncOpRef<expected<bool> > DeviceHandler::checkDoesFileExist(const zypp::filesystem::Pathname &filename)
  {
    zypp::PathInfo info( localPath( filename ) );
    if( info.isDir() ) {
      return makeReadyResult( expected<bool>::error( ZYPP_EXCPT_PTR(zypp::media::MediaNotAFileException(url(), localPath(filename)))) );
    }
    return makeReadyDeviceOp( shared_this<DeviceHandler>(), expected<bool>::success( info.isExist() ) );
  }

  expected<void> DeviceHandler::releasePath( zypp::filesystem::Pathname pathname )
  {
    if ( ! _doesLocalMirror || _attachPoint->empty() )
      return expected<void>::success();

    zypp::PathInfo info( localPath( pathname ) );

    try {
      if ( info.isFile() ) {
        zypp::filesystem::unlink( info.path() );
      } else if ( info.isDir() ) {
        if ( info.path() != localRoot() ) {
          zypp::filesystem::recursive_rmdir( info.path() );
        } else {
          zypp::filesystem::clean_dir( info.path() );
        }
      }
    }  catch ( const zypp::Exception &excp ) {
      ZYPP_CAUGHT( excp );
      return expected<void>::error( std::current_exception() );
    }

    return expected<void>::success();
  }

  AsyncOpRef<expected<PathRef>> DeviceHandler::provideDir(const zypp::Pathname & dirname, bool recurse_r)
  {
    if ( _attachPoint->empty() )
      return makeReadyResult( expected<PathRef>::error( ZYPP_EXCPT_PTR(zypp::media::MediaNotAttachedException( _url )) ) );
    return getDir( dirname, recurse_r );
  }

  AsyncOpRef< expected<PathRef> > DeviceHandler::provideFile(const zypp::OnMediaLocation &file)
  {
    if ( _attachPoint->empty() )
      return makeReadyResult( expected<PathRef>::error( ZYPP_EXCPT_PTR(zypp::media::MediaNotAttachedException( _url )) ) );
    return getFile( file );
  }

  AsyncOpRef<expected<bool>> DeviceHandler::doesFileExist(const zypp::Pathname & filename)
  {
    if ( _attachPoint->empty() )
      return makeReadyResult( expected<bool>::error( ZYPP_EXCPT_PTR(zypp::media::MediaNotAttachedException( _url )) ) );
    return checkDoesFileExist( filename );
  }

  SignalProxy<void ()> DeviceHandler::sigShutdown()
  {
    return _sigShutdown;
  }

  void DeviceHandler::immediateShutdown()
  {
    // tell all references to immediately turn invalid
    _sigShutdown.emit();
    _deviceShutdownConn.disconnect();
    _attachPoint.reset();
    _myDevice->delMount( _myMountId );
  }

}

