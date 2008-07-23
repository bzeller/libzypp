/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoManager.cc
 *
*/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <list>
#include <map>
#include <algorithm>
#include "zypp/base/InputStream.h"
#include "zypp/base/Logger.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/Function.h"
#include "zypp/base/Regex.h"
#include "zypp/PathInfo.h"
#include "zypp/TmpPath.h"
#include "zypp/Service.h"

#include "zypp/repo/RepoException.h"
#include "zypp/RepoManager.h"

#include "zypp/media/MediaManager.h"
#include "zypp/MediaSetAccess.h"
#include "zypp/ExternalProgram.h"
#include "zypp/ManagedFile.h"

#include "zypp/parser/RepoFileReader.h"
#include "zypp/parser/ServiceFileReader.h"
#include "zypp/parser/RepoindexFileReader.h"
#include "zypp/repo/yum/Downloader.h"
#include "zypp/repo/susetags/Downloader.h"
#include "zypp/parser/plaindir/RepoParser.h"

#include "zypp/ZYppCallbacks.h"

#include "sat/Pool.h"
#include "satsolver/pool.h"
#include "satsolver/repo.h"
#include "satsolver/repo_solv.h"

using namespace std;
using namespace zypp;
using namespace zypp::repo;
using namespace zypp::filesystem;

using namespace zypp::repo;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoManagerOptions
  //
  ///////////////////////////////////////////////////////////////////

  RepoManagerOptions::RepoManagerOptions( const Pathname & root_r )
  {
    repoCachePath         = Pathname::assertprefix( root_r, ZConfig::instance().repoCachePath() );
    repoRawCachePath      = Pathname::assertprefix( root_r, ZConfig::instance().repoMetadataPath() );
    repoSolvCachePath     = Pathname::assertprefix( root_r, ZConfig::instance().repoSolvfilesPath() );
    repoPackagesCachePath = Pathname::assertprefix( root_r, ZConfig::instance().repoPackagesPath() );
    knownReposPath        = Pathname::assertprefix( root_r, ZConfig::instance().knownReposPath() );
    knownServicesPath     = Pathname::assertprefix( root_r, ZConfig::instance().knownServicesPath() );
    probe                 = ZConfig::instance().repo_add_probe();
  }

  RepoManagerOptions RepoManagerOptions::makeTestSetup( const Pathname & root_r )
  {
    RepoManagerOptions ret;
    ret.repoCachePath         = root_r;
    ret.repoRawCachePath      = root_r/"raw";
    ret.repoSolvCachePath     = root_r/"solv";
    ret.repoPackagesCachePath = root_r/"packages";
    ret.knownReposPath        = root_r/"repos.d";
    ret.knownServicesPath     = root_r/"services.d";
    return ret;
  }

  ////////////////////////////////////////////////////////////////////////////

  /**
    * \short Simple callback to collect the results
    *
    * Classes like RepoFileParser call the callback
    * once per each repo in a file.
    *
    * Passing this functor as callback, you can collect
    * all resuls at the end, without dealing with async
    * code.
    */
    struct RepoCollector
    {
      RepoCollector()
      {}

      ~RepoCollector()
      {}

      bool collect( const RepoInfo &repo )
      {
        repos.push_back(repo);
        return true;
      }

      RepoInfoList repos;
    };

  ////////////////////////////////////////////////////////////////////////////

  /**
   * Reads RepoInfo's from a repo file.
   *
   * \param file pathname of the file to read.
   */
  static std::list<RepoInfo> repositories_in_file( const Pathname & file )
  {
    MIL << "repo file: " << file << endl;
    RepoCollector collector;
    parser::RepoFileReader parser( file, bind( &RepoCollector::collect, &collector, _1 ) );
    return collector.repos;
  }

  ////////////////////////////////////////////////////////////////////////////

  std::list<RepoInfo> readRepoFile(const Url & repo_file)
   {
     // no interface to download a specific file, using workaround:
     //! \todo add MediaManager::provideFile(Url file_url) to easily access any file URLs? (no need for media access id or media_nr)
     Url url(repo_file);
     Pathname path(url.getPathName());
     url.setPathName ("/");
     MediaSetAccess access(url);
     Pathname local = access.provideFile(path);

     DBG << "reading repo file " << repo_file << ", local path: " << local << endl;

     return repositories_in_file(local);
   }

  ////////////////////////////////////////////////////////////////////////////

  /**
   * \short List of RepoInfo's from a directory
   *
   * Goes trough every file ending with ".repo" in a directory and adds all
   * RepoInfo's contained in that file.
   *
   * \param dir pathname of the directory to read.
   */
  static std::list<RepoInfo> repositories_in_dir( const Pathname &dir )
  {
    MIL << "directory " << dir << endl;
    list<RepoInfo> repos;
    list<Pathname> entries;
    if ( filesystem::readdir( entries, Pathname(dir), false ) != 0 )
      ZYPP_THROW(Exception("failed to read directory"));

    str::regex allowedRepoExt("^\\.repo(_[0-9]+)?$");
    for ( list<Pathname>::const_iterator it = entries.begin(); it != entries.end(); ++it )
    {
      if (str::regex_match(it->extension(), allowedRepoExt))
      {
        list<RepoInfo> tmp = repositories_in_file( *it );
        repos.insert( repos.end(), tmp.begin(), tmp.end() );

        //std::copy( collector.repos.begin(), collector.repos.end(), std::back_inserter(repos));
        //MIL << "ok" << endl;
      }
    }
    return repos;
  }

  ////////////////////////////////////////////////////////////////////////////

  static void assert_alias( const RepoInfo &info )
  {
    if (info.alias().empty())
        ZYPP_THROW(RepoNoAliasException());
  }

  ////////////////////////////////////////////////////////////////////////////

  static void assert_urls( const RepoInfo &info )
  {
    if (info.baseUrlsEmpty())
        ZYPP_THROW(RepoNoUrlException());
  }

  ////////////////////////////////////////////////////////////////////////////

  /**
   * \short Calculates the raw cache path for a repository
   */
  static Pathname rawcache_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    assert_alias(info);
    return opt.repoRawCachePath / info.escaped_alias();
  }

  /**
   * \short Calculates the packages cache path for a repository
   */
  static Pathname packagescache_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    assert_alias(info);
    return opt.repoPackagesCachePath / info.escaped_alias();
  }

  static Pathname solv_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info)
  {
    assert_alias(info);
    return opt.repoSolvCachePath / info.escaped_alias();
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoManager::Impl
  //
  ///////////////////////////////////////////////////////////////////

  /**
   * \short RepoManager implementation.
   */
  struct RepoManager::Impl
  {
    Impl( const RepoManagerOptions &opt )
      : options(opt)
    {
      knownServices();
      knownRepositories();
    }

    Impl()
    {

    }

    RepoManagerOptions options;

    RepoSet repos;

    ServiceSet services;

  public:
    /** Offer default Impl. */
    static shared_ptr<Impl> nullimpl()
    {
      static shared_ptr<Impl> _nullimpl( new Impl );
      return _nullimpl;
    }

    void saveService( const Service& service ) const;

    Pathname generateNonExistingName( const Pathname &dir,
                                         const std::string &basefilename ) const;

    std::string generateFilename( const RepoInfo &info ) const;
    std::string generateFilename( const Service &info ) const;

    struct ServiceCollector {
      ServiceCollector(ServiceSet& services_) : services(services_) {}
      bool collect(Service service) { services.insert(service); return true; }
      private:
        ServiceSet& services;
    };

    void knownServices();

    void knownRepositories();

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( *this ); }
  };

  ///////////////////////////////////////////////////////////////////

  /** \relates RepoManager::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const RepoManager::Impl & obj )
  {
    return str << "RepoManager::Impl";
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoManager
  //
  ///////////////////////////////////////////////////////////////////

  RepoManager::RepoManager( const RepoManagerOptions &opt )
  : _pimpl( new Impl(opt) )
  {}

  ////////////////////////////////////////////////////////////////////////////

  RepoManager::~RepoManager()
  {}

  ////////////////////////////////////////////////////////////////////////////

  bool RepoManager::repoEmpty() const { return _pimpl->repos.empty(); }
  RepoManager::RepoSizeType RepoManager::repoSize() const
  { return _pimpl->repos.size(); }
  RepoManager::RepoConstIterator RepoManager::repoBegin() const
  { return _pimpl->repos.begin(); }
  RepoManager::RepoConstIterator RepoManager::repoEnd() const
  { return _pimpl->repos.end(); }


  std::list<RepoInfo> RepoManager::knownRepositories() const
  {
    return std::list<RepoInfo>(repoBegin(),repoEnd());
  }

  void RepoManager::Impl::knownRepositories()
  {
    MIL << "start construct known repos" << endl;

    if ( PathInfo(options.knownReposPath).isExist() )
    {
      RepoInfoList repol = repositories_in_dir(options.knownReposPath);
      for ( RepoInfoList::iterator it = repol.begin();
            it != repol.end();
            ++it )
      {
        // set the metadata path for the repo
        Pathname metadata_path = rawcache_path_for_repoinfo(options, (*it));
        (*it).setMetadataPath(metadata_path);

	// set the downloaded packages path for the repo
	Pathname packages_path = packagescache_path_for_repoinfo(options, (*it));
	(*it).setPackagesPath(packages_path);

        repos.insert(*it);
      }
    }

    MIL << "end construct known repos" << endl;
  }

  ////////////////////////////////////////////////////////////////////////////

  Pathname RepoManager::metadataPath( const RepoInfo &info ) const
  {
    return rawcache_path_for_repoinfo(_pimpl->options, info );
  }

  Pathname RepoManager::packagesPath( const RepoInfo &info ) const
  {
    return packagescache_path_for_repoinfo(_pimpl->options, info );
  }

  ////////////////////////////////////////////////////////////////////////////

  RepoStatus RepoManager::metadataStatus( const RepoInfo &info ) const
  {
    Pathname rawpath = rawcache_path_for_repoinfo( _pimpl->options, info );
    RepoType repokind = info.type();
    RepoStatus status;

    switch ( repokind.toEnum() )
    {
      case RepoType::NONE_e:
      // unknown, probe the local metadata
        repokind = probe(rawpath.asUrl());
      break;
      default:
      break;
    }

    switch ( repokind.toEnum() )
    {
      case RepoType::RPMMD_e :
      {
        status = RepoStatus( rawpath + "/repodata/repomd.xml");
      }
      break;

      case RepoType::YAST2_e :
      {
        status = RepoStatus( rawpath + "/content") && (RepoStatus( rawpath + "/media.1/media"));
      }
      break;

      case RepoType::RPMPLAINDIR_e :
      {
        if ( PathInfo(Pathname(rawpath + "/cookie")).isExist() )
          status = RepoStatus( rawpath + "/cookie");
      }
      break;

      case RepoType::NONE_e :
	// Return default RepoStatus in case of RepoType::NONE
	// indicating it should be created?
        // ZYPP_THROW(RepoUnknownTypeException());
	break;
    }
    return status;
  }

  void RepoManager::touchIndexFile(const RepoInfo & info)
  {
    Pathname rawpath = rawcache_path_for_repoinfo( _pimpl->options, info );

    RepoType repokind = info.type();
    if ( repokind.toEnum() == RepoType::NONE_e )
      // unknown, probe the local metadata
      repokind = probe(rawpath.asUrl());
    // if still unknown, just return
    if (repokind == RepoType::NONE_e)
      return;

    Pathname p;
    switch ( repokind.toEnum() )
    {
      case RepoType::RPMMD_e :
        p = Pathname(rawpath + "/repodata/repomd.xml");
        break;

      case RepoType::YAST2_e :
        p = Pathname(rawpath + "/content");
        break;

      case RepoType::RPMPLAINDIR_e :
        p = Pathname(rawpath + "/cookie");
        break;

      case RepoType::NONE_e :
      default:
        break;
    }

    // touch the file, ignore error (they are logged anyway)
    filesystem::touch(p);
  }

  RepoManager::RefreshCheckStatus RepoManager::checkIfToRefreshMetadata(
                                              const RepoInfo &info,
                                              const Url &url,
                                              RawMetadataRefreshPolicy policy )
  {
    assert_alias(info);

    RepoStatus oldstatus;
    RepoStatus newstatus;

    try
    {
      MIL << "Going to try to check whether refresh is needed for " << url << endl;

      repo::RepoType repokind = info.type();

      // if the type is unknown, try probing.
      switch ( repokind.toEnum() )
      {
        case RepoType::NONE_e:
          // unknown, probe it
          repokind = probe(url);
        break;
        default:
        break;
      }

      Pathname rawpath = rawcache_path_for_repoinfo( _pimpl->options, info );
      filesystem::assert_dir(rawpath);
      oldstatus = metadataStatus(info);

      // now we've got the old (cached) status, we can decide repo.refresh.delay
      if (policy != RefreshForced && policy != RefreshIfNeededIgnoreDelay)
      {
        // difference in seconds
        double diff = difftime(
          (Date::ValueType)Date::now(),
          (Date::ValueType)oldstatus.timestamp()) / 60;

        DBG << "oldstatus: " << (Date::ValueType)oldstatus.timestamp() << endl;
        DBG << "current time: " << (Date::ValueType)Date::now() << endl;
        DBG << "last refresh = " << diff << " minutes ago" << endl;

        if (diff < ZConfig::instance().repo_refresh_delay())
        {
          MIL << "Repository '" << info.alias()
              << "' has been refreshed less than repo.refresh.delay ("
              << ZConfig::instance().repo_refresh_delay()
              << ") minutes ago. Advising to skip refresh" << endl;
          return REPO_CHECK_DELAYED;
        }
      }

      // create temp dir as sibling of rawpath
      filesystem::TmpDir tmpdir( filesystem::TmpDir::makeSibling( rawpath ) );

      if ( ( repokind.toEnum() == RepoType::RPMMD_e ) ||
           ( repokind.toEnum() == RepoType::YAST2_e ) )
      {
        MediaSetAccess media(url);
        shared_ptr<repo::Downloader> downloader_ptr;

        if ( repokind.toEnum() == RepoType::RPMMD_e )
          downloader_ptr.reset(new yum::Downloader(info.path()));
        else
          downloader_ptr.reset( new susetags::Downloader(info.path()));

        RepoStatus newstatus = downloader_ptr->status(media);
        bool refresh = false;
        if ( oldstatus.checksum() == newstatus.checksum() )
        {
          MIL << "repo has not changed" << endl;
          if ( policy == RefreshForced )
          {
            MIL << "refresh set to forced" << endl;
            refresh = true;
          }
        }
        else
        {
          MIL << "repo has changed, going to refresh" << endl;
          refresh = true;
        }

        if (!refresh)
          touchIndexFile(info);

        return refresh ? REFRESH_NEEDED : REPO_UP_TO_DATE;
      }
      else if ( repokind.toEnum() == RepoType::RPMPLAINDIR_e )
      {
        RepoStatus newstatus = parser::plaindir::dirStatus(url.getPathName());
        bool refresh = false;
        if ( oldstatus.checksum() == newstatus.checksum() )
        {
          MIL << "repo has not changed" << endl;
          if ( policy == RefreshForced )
          {
            MIL << "refresh set to forced" << endl;
            refresh = true;
          }
        }
        else
        {
          MIL << "repo has changed, going to refresh" << endl;
          refresh = true;
        }

        if (!refresh)
          touchIndexFile(info);

        return refresh ? REFRESH_NEEDED : REPO_UP_TO_DATE;
      }
      else
      {
        ZYPP_THROW(RepoUnknownTypeException(info));
      }
    }
    catch ( const Exception &e )
    {
      ZYPP_CAUGHT(e);
      ERR << "refresh check failed for " << url << endl;
      ZYPP_RETHROW(e);
    }

    return REFRESH_NEEDED; // default
  }

  void RepoManager::refreshMetadata( const RepoInfo &info,
                                     RawMetadataRefreshPolicy policy,
                                     const ProgressData::ReceiverFnc & progress )
  {
    assert_alias(info);
    assert_urls(info);

    // we will throw this later if no URL checks out fine
    RepoException rexception(_("Valid metadata not found at specified URL(s)"));

    // try urls one by one
    for ( RepoInfo::urls_const_iterator it = info.baseUrlsBegin(); it != info.baseUrlsEnd(); ++it )
    {
      try
      {
        Url url(*it);

        // check whether to refresh metadata
        // if the check fails for this url, it throws, so another url will be checked
        if (checkIfToRefreshMetadata(info, url, policy)!=REFRESH_NEEDED)
          return;

        MIL << "Going to refresh metadata from " << url << endl;

        repo::RepoType repokind = info.type();

        // if the type is unknown, try probing.
        switch ( repokind.toEnum() )
        {
          case RepoType::NONE_e:
            // unknown, probe it
            repokind = probe(*it);

            if (repokind.toEnum() != RepoType::NONE_e)
            {
              // Adjust the probed type in RepoInfo
              info.setProbedType( repokind ); // lazy init!
              //save probed type only for repos in system
              for_( it, repoBegin(), repoEnd() )
              {
                if ( info.alias() == (*it).alias() )
                {
                  RepoInfo modifiedrepo = info;
                  modifiedrepo.setType(repokind);
                  modifyRepository(info.alias(),modifiedrepo);
                }
              }
            }
          break;
          default:
          break;
        }

        Pathname rawpath = rawcache_path_for_repoinfo( _pimpl->options, info );
        filesystem::assert_dir(rawpath);

        // create temp dir as sibling of rawpath
        filesystem::TmpDir tmpdir( filesystem::TmpDir::makeSibling( rawpath ) );

        if ( ( repokind.toEnum() == RepoType::RPMMD_e ) ||
             ( repokind.toEnum() == RepoType::YAST2_e ) )
        {
          MediaSetAccess media(url);
          shared_ptr<repo::Downloader> downloader_ptr;

          if ( repokind.toEnum() == RepoType::RPMMD_e )
            downloader_ptr.reset(new yum::Downloader(info.path()));
          else
            downloader_ptr.reset( new susetags::Downloader(info.path()));

          /**
           * Given a downloader, sets the other repos raw metadata
           * path as cache paths for the fetcher, so if another
           * repo has the same file, it will not download it
           * but copy it from the other repository
           */
          for_( it, repoBegin(), repoEnd() )
          {
            Pathname cachepath(rawcache_path_for_repoinfo( _pimpl->options, *it ));
            if ( PathInfo(cachepath).isExist() )
              downloader_ptr->addCachePath(cachepath);
          }

          downloader_ptr->download( media, tmpdir.path());
        }
        else if ( repokind.toEnum() == RepoType::RPMPLAINDIR_e )
        {
          RepoStatus newstatus = parser::plaindir::dirStatus(url.getPathName());

          std::ofstream file(( tmpdir.path() + "/cookie").c_str());
          if (!file) {
            ZYPP_THROW (Exception( "Can't open " + tmpdir.path().asString() + "/cookie" ) );
          }
          file << url << endl;
          file << newstatus.checksum() << endl;

          file.close();
        }
        else
        {
          ZYPP_THROW(RepoUnknownTypeException());
        }

        // ok we have the metadata, now exchange
        // the contents

        TmpDir oldmetadata( TmpDir::makeSibling( rawpath ) );
        filesystem::rename( rawpath, oldmetadata.path() );
        // move the just downloaded there
        filesystem::rename( tmpdir.path(), rawpath );

        // we are done.
        return;
      }
      catch ( const Exception &e )
      {
        ZYPP_CAUGHT(e);
        ERR << "Trying another url..." << endl;

        // remember the exception caught for the *first URL*
        // if all other URLs fail, the rexception will be thrown with the
        // cause of the problem of the first URL remembered
        if (it == info.baseUrlsBegin())
          rexception.remember(e);
      }
    } // for every url
    ERR << "No more urls..." << endl;
    ZYPP_THROW(rexception);
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::cleanMetadata( const RepoInfo &info,
                                   const ProgressData::ReceiverFnc & progressfnc )
  {
    ProgressData progress(100);
    progress.sendTo(progressfnc);

    filesystem::recursive_rmdir(rawcache_path_for_repoinfo(_pimpl->options, info));
    progress.toMax();
  }

  void RepoManager::cleanPackages( const RepoInfo &info,
                                   const ProgressData::ReceiverFnc & progressfnc )
  {
    ProgressData progress(100);
    progress.sendTo(progressfnc);

    filesystem::recursive_rmdir(packagescache_path_for_repoinfo(_pimpl->options, info));
    progress.toMax();
  }

  void RepoManager::buildCache( const RepoInfo &info,
                                CacheBuildPolicy policy,
                                const ProgressData::ReceiverFnc & progressrcv )
  {
    assert_alias(info);
    Pathname rawpath = rawcache_path_for_repoinfo(_pimpl->options, info);

    filesystem::assert_dir(_pimpl->options.repoCachePath);
    RepoStatus raw_metadata_status = metadataStatus(info);
    if ( raw_metadata_status.empty() )
    {
       /* if there is no cache at this point, we refresh the raw
          in case this is the first time - if it's !autorefresh,
          we may still refresh */
      refreshMetadata(info, RefreshIfNeeded, progressrcv );
      raw_metadata_status = metadataStatus(info);
    }

    bool needs_cleaning = false;
    if ( isCached( info ) )
    {
      MIL << info.alias() << " is already cached." << endl;
      RepoStatus cache_status = cacheStatus(info);

      if ( cache_status.checksum() == raw_metadata_status.checksum() )
      {
        MIL << info.alias() << " cache is up to date with metadata." << endl;
        if ( policy == BuildIfNeeded ) {
          return;
        }
        else {
          MIL << info.alias() << " cache rebuild is forced" << endl;
        }
      }

      needs_cleaning = true;
    }

    ProgressData progress(100);
    callback::SendReport<ProgressReport> report;
    progress.sendTo( ProgressReportAdaptor( progressrcv, report ) );
    progress.name(str::form(_("Building repository '%s' cache"), info.name().c_str()));
    progress.toMin();

    if (needs_cleaning)
    {
      cleanCache(info);
    }

    MIL << info.alias() << " building cache..." << endl;

    Pathname base = solv_path_for_repoinfo( _pimpl->options, info);
    filesystem::assert_dir(base);
    Pathname solvfile = base / "solv";

    // do we have type?
    repo::RepoType repokind = info.type();

    // if the type is unknown, try probing.
    switch ( repokind.toEnum() )
    {
      case RepoType::NONE_e:
        // unknown, probe the local metadata
        repokind = probe(rawpath.asUrl());
      break;
      default:
      break;
    }

    MIL << "repo type is " << repokind << endl;

    switch ( repokind.toEnum() )
    {
      case RepoType::RPMMD_e :
      case RepoType::YAST2_e :
      case RepoType::RPMPLAINDIR_e :
      {
        // Take care we unlink the solvfile on exception
        ManagedFile guard( solvfile, filesystem::unlink );

        ostringstream cmd;
        std::string toFile( str::gsub(solvfile.asString(),"\"","\\\"") );
        if ( repokind.toEnum() == RepoType::RPMPLAINDIR_e )
        {
          // FIXME this does only work form dir: URLs
          cmd << str::form( "repo2solv.sh \"%s\" > \"%s\"",
                            str::gsub( info.baseUrlsBegin()->getPathName(),"\"","\\\"" ).c_str(),
                            toFile.c_str() );
        }
        else
        {
          cmd << str::form( "repo2solv.sh \"%s\" > \"%s\"",
                            str::gsub( rawpath.asString(),"\"","\\\"" ).c_str(),
                            toFile.c_str() );
        }
        MIL << "Executing: " << cmd.str() << endl;
        ExternalProgram prog( cmd.str(), ExternalProgram::Stderr_To_Stdout );

        cmd << endl;
        for ( string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() ) {
          WAR << "  " << output;
          cmd << "     " << output;
        }

        int ret = prog.close();
        if ( ret != 0 )
        {
          RepoException ex(str::form("Failed to cache repo (%d).", ret));
          ex.remember( cmd.str() );
          ZYPP_THROW(ex);
        }

        // We keep it.
        guard.resetDispose();
      }
      break;
      default:
        ZYPP_THROW(RepoUnknownTypeException("Unhandled repository type"));
      break;
    }
    // update timestamp and checksum
    setCacheStatus(info, raw_metadata_status);
    MIL << "Commit cache.." << endl;
    progress.toMax();
  }

  ////////////////////////////////////////////////////////////////////////////

  repo::RepoType RepoManager::probe( const Url &url ) const
  {
    if ( url.getScheme() == "dir" && ! PathInfo( url.getPathName() ).isDir() )
    {
      // Handle non existing local directory in advance, as
      // MediaSetAccess does not support it.
      return repo::RepoType::NONE;
    }

    try
    {
      MediaSetAccess access(url);
      if ( access.doesFileExist("/repodata/repomd.xml") )
        return repo::RepoType::RPMMD;
      if ( access.doesFileExist("/content") )
        return repo::RepoType::YAST2;

      // if it is a local url of type dir
      if ( (! media::MediaManager::downloads(url)) && ( url.getScheme() == "dir" ) )
      {
        Pathname path = Pathname(url.getPathName());
        if ( PathInfo(path).isDir() )
        {
          // allow empty dirs for now
          return repo::RepoType::RPMPLAINDIR;
        }
      }
    }
    catch ( const media::MediaException &e )
    {
      ZYPP_CAUGHT(e);
      RepoException enew("Error trying to read from " + url.asString());
      enew.remember(e);
      ZYPP_THROW(enew);
    }
    catch ( const Exception &e )
    {
      ZYPP_CAUGHT(e);
      Exception enew("Unknown error reading from " + url.asString());
      enew.remember(e);
      ZYPP_THROW(enew);
    }

    return repo::RepoType::NONE;
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::cleanCache( const RepoInfo &info,
                                const ProgressData::ReceiverFnc & progressrcv )
  {
    ProgressData progress(100);
    progress.sendTo(progressrcv);
    progress.toMin();

    filesystem::recursive_rmdir(solv_path_for_repoinfo(_pimpl->options, info));

    progress.toMax();
  }

  ////////////////////////////////////////////////////////////////////////////

  bool RepoManager::isCached( const RepoInfo &info ) const
  {
    return PathInfo(solv_path_for_repoinfo( _pimpl->options, info ) / "solv").isExist();
  }

  RepoStatus RepoManager::cacheStatus( const RepoInfo &info ) const
  {

    Pathname cookiefile = solv_path_for_repoinfo(_pimpl->options, info) / "cookie";

    return RepoStatus::fromCookieFile(cookiefile);
  }

  void RepoManager::setCacheStatus( const RepoInfo &info, const RepoStatus &status )
  {
    Pathname base = solv_path_for_repoinfo(_pimpl->options, info);
    filesystem::assert_dir(base);
    Pathname cookiefile = base / "cookie";

    status.saveToCookieFile(cookiefile);
  }

  void RepoManager::loadFromCache( const RepoInfo & info,
                                   const ProgressData::ReceiverFnc & progressrcv )
  {
    assert_alias(info);
    Pathname solvfile = solv_path_for_repoinfo(_pimpl->options, info) / "solv";

    if ( ! PathInfo(solvfile).isExist() )
      ZYPP_THROW(RepoNotCachedException(info));

    try
    {
      sat::Pool::instance().addRepoSolv( solvfile, info );
    }
    catch ( const Exception & exp )
    {
      ZYPP_CAUGHT( exp );
      MIL << "Try to handle exception by rebuilding the solv-file" << endl;
      cleanCache( info, progressrcv );
      buildCache( info, BuildIfNeeded, progressrcv );

      sat::Pool::instance().addRepoSolv( solvfile, info );
    }
  }

  ////////////////////////////////////////////////////////////////////////////

  /**
   * Generate a non existing filename in a directory, using a base
   * name. For example if a directory contains 3 files
   *
   * |-- bar
   * |-- foo
   * `-- moo
   *
   * If you try to generate a unique filename for this directory,
   * based on "ruu" you will get "ruu", but if you use the base
   * "foo" you will get "foo_1"
   *
   * \param dir Directory where the file needs to be unique
   * \param basefilename string to base the filename on.
   */
  Pathname RepoManager::Impl::generateNonExistingName( const Pathname &dir,
                                       const std::string &basefilename ) const
  {
    string final_filename = basefilename;
    int counter = 1;
    while ( PathInfo(dir + final_filename).isExist() )
    {
      final_filename = basefilename + "_" + str::numstring(counter);
      counter++;
    }
    return dir + Pathname(final_filename);
  }

  ////////////////////////////////////////////////////////////////////////////

  /**
   * \short Generate a related filename from a repo info
   *
   * From a repo info, it will try to use the alias as a filename
   * escaping it if necessary. Other fallbacks can be added to
   * this function in case there is no way to use the alias
   */
  std::string RepoManager::Impl::generateFilename( const RepoInfo &info ) const
  {
    std::string filename = info.alias();
    // replace slashes with underscores
    str::replaceAll( filename, "/", "_" );

    filename = Pathname(filename).extend(".repo").asString();
    MIL << "generating filename for repo [" << info.alias() << "] : '" << filename << "'" << endl;
    return filename;
  }

  std::string RepoManager::Impl::generateFilename( const Service &info ) const
  {
    std::string filename = info.name();
    // replace slashes with underscores
    str::replaceAll( filename, "/", "_" );

    filename = Pathname(filename).extend(".service").asString();
    MIL << "generating filename for service [" << info.name() << "] : '" << filename << "'" << endl;
    return filename;
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::addRepository( const RepoInfo &info,
                                   const ProgressData::ReceiverFnc & progressrcv )
  {
    assert_alias(info);

    ProgressData progress(100);
    callback::SendReport<ProgressReport> report;
    progress.sendTo( ProgressReportAdaptor( progressrcv, report ) );
    progress.name(str::form(_("Adding repository '%s'"), info.name().c_str()));
    progress.toMin();

    RepoInfo tosave = info;
    if(_pimpl->repos.find(tosave)!= _pimpl->repos.end())
        ZYPP_THROW(RepoAlreadyExistsException(info));


    // check the first url for now
    if ( _pimpl->options.probe )
    {
      DBG << "unknown repository type, probing" << endl;

      RepoType probedtype;
      probedtype = probe(*tosave.baseUrlsBegin());
      if ( tosave.baseUrlsSize() > 0 )
      {
        if ( probedtype == RepoType::NONE )
          ZYPP_THROW(RepoUnknownTypeException());
        else
          tosave.setType(probedtype);
      }
    }

    progress.set(50);

    // assert the directory exists
    filesystem::assert_dir(_pimpl->options.knownReposPath);

    Pathname repofile = _pimpl->generateNonExistingName(
        _pimpl->options.knownReposPath, _pimpl->generateFilename(tosave));
    // now we have a filename that does not exists
    MIL << "Saving repo in " << repofile << endl;

    std::ofstream file(repofile.c_str());
    if (!file) {
      ZYPP_THROW (Exception( "Can't open " + repofile.asString() ) );
    }

    tosave.dumpRepoOn(file);
    tosave.setFilepath(repofile);
    _pimpl->repos.insert(tosave);
    progress.toMax();
    MIL << "done" << endl;
  }

  void RepoManager::addRepositories( const Url &url,
                                     const ProgressData::ReceiverFnc & progressrcv )
  {
    std::list<RepoInfo> repos = readRepoFile(url);
    for ( std::list<RepoInfo>::const_iterator it = repos.begin();
          it != repos.end();
          ++it )
    {
      // look if the alias is in the known repos.
      for_ ( kit, repoBegin(), repoEnd() )
      {
        if ( (*it).alias() == (*kit).alias() )
        {
          ERR << "To be added repo " << (*it).alias() << " conflicts with existing repo " << (*kit).alias() << endl;
          ZYPP_THROW(RepoAlreadyExistsException(*it));
        }
      }
    }

    string filename = Pathname(url.getPathName()).basename();

    if ( filename == Pathname() )
      ZYPP_THROW(RepoException("Invalid repo file name at " + url.asString() ));

    // assert the directory exists
    filesystem::assert_dir(_pimpl->options.knownReposPath);

    Pathname repofile = _pimpl->generateNonExistingName(_pimpl->options.knownReposPath, filename);
    // now we have a filename that does not exists
    MIL << "Saving " << repos.size() << " repo" << ( repos.size() ? "s" : "" ) << " in " << repofile << endl;

    std::ofstream file(repofile.c_str());
    if (!file) {
      ZYPP_THROW (Exception( "Can't open " + repofile.asString() ) );
    }

    for ( std::list<RepoInfo>::iterator it = repos.begin();
          it != repos.end();
          ++it )
    {
      MIL << "Saving " << (*it).alias() << endl;
      it->setFilepath(repofile.asString());
      it->dumpRepoOn(file);
      _pimpl->repos.insert(*it);
    }
    MIL << "done" << endl;
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::removeRepository( const RepoInfo & info,
                                      const ProgressData::ReceiverFnc & progressrcv)
  {
    ProgressData progress;
    callback::SendReport<ProgressReport> report;
    progress.sendTo( ProgressReportAdaptor( progressrcv, report ) );
    progress.name(str::form(_("Removing repository '%s'"), info.name().c_str()));

    MIL << "Going to delete repo " << info.alias() << endl;

    for_( it, repoBegin(), repoEnd() )
    {
      // they can be the same only if the provided is empty, that means
      // the provided repo has no alias
      // then skip
      if ( (!info.alias().empty()) && ( info.alias() != (*it).alias() ) )
        continue;

      // TODO match by url

      // we have a matcing repository, now we need to know
      // where it does come from.
      RepoInfo todelete = *it;
      if (todelete.filepath().empty())
      {
        ZYPP_THROW(RepoException("Can't figure where the repo is stored"));
      }
      else
      {
        // figure how many repos are there in the file:
        std::list<RepoInfo> filerepos = repositories_in_file(todelete.filepath());
        if ( (filerepos.size() == 1) && ( filerepos.front().alias() == todelete.alias() ) )
        {
          // easy, only this one, just delete the file
          if ( filesystem::unlink(todelete.filepath()) != 0 )
          {
            ZYPP_THROW(RepoException("Can't delete " + todelete.filepath().asString()));
          }
          MIL << todelete.alias() << " sucessfully deleted." << endl;
        }
        else
        {
          // there are more repos in the same file
          // write them back except the deleted one.
          //TmpFile tmp;
          //std::ofstream file(tmp.path().c_str());

          // assert the directory exists
          filesystem::assert_dir(todelete.filepath().dirname());

          std::ofstream file(todelete.filepath().c_str());
          if (!file) {
            //ZYPP_THROW (Exception( "Can't open " + tmp.path().asString() ) );
            ZYPP_THROW (Exception( "Can't open " + todelete.filepath().asString() ) );
          }
          for ( std::list<RepoInfo>::const_iterator fit = filerepos.begin();
                fit != filerepos.end();
                ++fit )
          {
            if ( (*fit).alias() != todelete.alias() )
              (*fit).dumpRepoOn(file);
          }
        }

        CombinedProgressData subprogrcv(progress, 70);
        CombinedProgressData cleansubprogrcv(progress, 30);
        // now delete it from cache
        if ( isCached(todelete) )
          cleanCache( todelete, subprogrcv);
        // now delete metadata (#301037)
        cleanMetadata( todelete, cleansubprogrcv);
        _pimpl->repos.erase(todelete);
        MIL << todelete.alias() << " sucessfully deleted." << endl;
        return;
      } // else filepath is empty

    }
    // should not be reached on a sucess workflow
    ZYPP_THROW(RepoNotFoundException(info));
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::modifyRepository( const std::string &alias,
                                      const RepoInfo & newinfo,
                                      const ProgressData::ReceiverFnc & progressrcv )
  {
    RepoInfo toedit = getRepositoryInfo(alias);

    // check if the new alias already exists when renaming the repo
    if (alias != newinfo.alias())
    {
      for_( it, repoBegin(), repoEnd() )
      {
        if ( newinfo.alias() == (*it).alias() )
          ZYPP_THROW(RepoAlreadyExistsException(newinfo));
      }
    }

    if (toedit.filepath().empty())
    {
      ZYPP_THROW(RepoException("Can't figure where the repo is stored"));
    }
    else
    {
      // figure how many repos are there in the file:
      std::list<RepoInfo> filerepos = repositories_in_file(toedit.filepath());

      // there are more repos in the same file
      // write them back except the deleted one.
      //TmpFile tmp;
      //std::ofstream file(tmp.path().c_str());

      // assert the directory exists
      filesystem::assert_dir(toedit.filepath().dirname());

      std::ofstream file(toedit.filepath().c_str());
      if (!file) {
        //ZYPP_THROW (Exception( "Can't open " + tmp.path().asString() ) );
        ZYPP_THROW (Exception( "Can't open " + toedit.filepath().asString() ) );
      }
      for ( std::list<RepoInfo>::const_iterator fit = filerepos.begin();
            fit != filerepos.end();
            ++fit )
      {
          // if the alias is different, dump the original
          // if it is the same, dump the provided one
          if ( (*fit).alias() != toedit.alias() )
            (*fit).dumpRepoOn(file);
          else
            newinfo.dumpRepoOn(file);
      }

      _pimpl->repos.erase(toedit);
      _pimpl->repos.insert(newinfo);
    }
  }

  ////////////////////////////////////////////////////////////////////////////

  RepoInfo RepoManager::getRepositoryInfo( const std::string &alias,
                                           const ProgressData::ReceiverFnc & progressrcv )
  {
    RepoInfo info;
    info.setAlias(alias);
    RepoConstIterator it = _pimpl->repos.find( info );
    if( it == repoEnd() )
      ZYPP_THROW(RepoNotFoundException(info));
    else
      return *it;
  }

  ////////////////////////////////////////////////////////////////////////////

  RepoInfo RepoManager::getRepositoryInfo( const Url & url,
                                           const url::ViewOption & urlview,
                                           const ProgressData::ReceiverFnc & progressrcv )
  {
    for_( it, repoBegin(), repoEnd() )
    {
      for(RepoInfo::urls_const_iterator urlit = (*it).baseUrlsBegin();
          urlit != (*it).baseUrlsEnd();
          ++urlit)
      {
        if ((*urlit).asString(urlview) == url.asString(urlview))
          return *it;
      }
    }
    RepoInfo info;
    info.setBaseUrl(url);
    ZYPP_THROW(RepoNotFoundException(info));
  }

  void RepoManager::addService( const std::string& name, const Url& url )
  {
    addService( Service(name,url) );
  }

  void RepoManager::addService( const Service& service )
  {
    //check if service isn't already exist
    if( _pimpl->services.find(service)!= _pimpl->services.end() )
      return; //FIXME ZYPP_THROW(RepoAlreadyExistsException(service.name()));

    //this is need to save location to correct service
    const Service& savedService = *(_pimpl->services.insert( service )).first;

    _pimpl->saveService( savedService );
  }

  void RepoManager::removeService( const string& name)
  {
    MIL << "Going to delete repo " << name << endl;

    const Service& service = getService( name );

    Pathname location = service.location();
    if( location.empty() )
    {
      ZYPP_THROW(RepoException("Can't figure where the service is stored"));
    }

    ServiceSet tmpSet;
    Impl::ServiceCollector collector(tmpSet);

    parser::ServiceFileReader reader( location,
        bind(&Impl::ServiceCollector::collect,collector,_1) );

    if ( tmpSet.size() == 1 ) //only one record
    {
      if ( filesystem::unlink(location) != 0 )
      {
        ZYPP_THROW(RepoException("Can't delete " + location.asString()));
      }
      MIL << name << " sucessfully deleted." << endl;
    }
    else
    {
      filesystem::assert_dir(location.dirname());

      std::ofstream file(location.c_str());
      if( file.fail() )
        ZYPP_THROW(Exception("failed open file to write"));

      for_(it, tmpSet.begin(), tmpSet.end())
      {
        if( it->name() != name )
          it->dumpServiceOn(file);
      }

      MIL << name << " sucessfully deleted from file " << location <<  endl;
    }

    //now remove all repositories added by this service
    getRepositoriesInService( name,
      boost::make_function_output_iterator(bind(&RepoManager::removeRepository, this, _1, ProgressData::ReceiverFnc()) ) );
  }


  void RepoManager::Impl::saveService( const Service& service ) const
  {
    filesystem::assert_dir( options.knownServicesPath );

    Pathname servfile = generateNonExistingName( options.knownServicesPath,
        generateFilename( service ) );

    MIL << "saving service in " << servfile << endl;

    std::ofstream file(servfile.c_str());
    if (!file) {
      ZYPP_THROW (Exception( "Can't open " + servfile.asString() ) );
    }

    service.dumpServiceOn( file );

    const_cast<Service&>(service).setLocation( servfile );
    MIL << "done" << endl;
  }

  Service RepoManager::getService( const std::string& name ) const
  {
    ServiceConstIterator it = _pimpl->services.find(name);
    if ( it == serviceEnd() )
      return Service::noService;
    else
      return *it;
  }

  bool RepoManager::serviceEmpty() const { return _pimpl->services.empty(); }

  RepoManager::ServiceSizeType RepoManager::serviceSize() const
  {
    return _pimpl->services.size();
  }

  RepoManager::ServiceConstIterator RepoManager::serviceBegin() const
  {
    return _pimpl->services.begin();
  }

  RepoManager::ServiceConstIterator RepoManager::serviceEnd() const
  {
    return _pimpl->services.end();
  }

  void RepoManager::refreshServices()
  {
    //cannot user for_, because it uses const version
    for (std::set<Service>::iterator it = _pimpl->services.begin();
      it != _pimpl->services.end(); ++it)
    {
      if ( !it->enabled() )
        continue;

      MIL << "refresh: " << it->name() << " with url: "<< it->url().asString() << endl;
      refreshService(*it);
    }
  }

  void RepoManager::refreshService( const Service& service )
  {
    //download index file
    media::MediaManager mediamanager;
    media::MediaAccessId mid = mediamanager.open( service.url() ); //FIXME check if url is not empty
    mediamanager.attachDesiredMedia( mid );
    mediamanager.provideFile( mid, "repo/repoindex.xml" );
    Pathname path = mediamanager.localPath(mid, "repo/repoindex.xml" );

    //parse it
    RepoCollector collector;
    parser::RepoindexFileReader reader( path,
      bind( &RepoCollector::collect, &collector, _1 ) );
    mediamanager.release( mid );
    mediamanager.close( mid );

    // set base url for all collected repositories
    for_( it, collector.repos.begin(), collector.repos.end())
    {
      it->setBaseUrl( service.url() );
      it->setService(service.name());
    }

    //compare old and new repositories (hope not to much, if it change
    // then construct set and use set operation on it)

    std::list<RepoInfo> oldRepos;
    getRepositoriesInService(service.name(),
        insert_iterator<std::list<RepoInfo> >
        (oldRepos,oldRepos.begin()));

    //find old to remove
    for_( it, oldRepos.begin(), oldRepos.end() )
    {
      bool found = false;

      for_( it2, collector.repos.begin(), collector.repos.end() )
        if ( it->alias() == it2->alias() )
        {
          found = true;
          break;
        }

      if( !found )
        removeRepository( *it );
    }

    //find new to add
    for_( it, collector.repos.begin(), collector.repos.end() )
    {
      bool found = false;

      for_( it2, oldRepos.begin(), oldRepos.end() )
        if( it->alias() == it2->alias() )
        {
          found = true;
          break;
        }

      if (!found)
        addRepository( *it );
    }

  }

  void RepoManager::modifyService(const std::string& oldName, const Service& service)
  {
    MIL << "Going to modify service " << oldName << endl;

    const Service& oldService = getService(oldName);

    Pathname location = oldService.location();
    if( location.empty() )
    {
      ZYPP_THROW(RepoException("Can't figure where the service is stored"));
    }

    ServiceSet tmpSet;
    Impl::ServiceCollector collector(tmpSet);

    parser::ServiceFileReader reader( location,
        bind(&Impl::ServiceCollector::collect,collector,_1) );

    filesystem::assert_dir(location.dirname());

    std::ofstream file(location.c_str());

    for_(it, tmpSet.begin(), tmpSet.end())
    {
      if( *it != oldName )
        it->dumpServiceOn(file);
    }

    service.dumpServiceOn(file);

    file.close();

    _pimpl->services.erase(oldName);
    _pimpl->services.insert(service);

    if( oldName != service.name() ) //changed name, must change also repository
    {
      std::vector<RepoInfo> toModify;
      getRepositoriesInService(oldName,
        insert_iterator<std::vector<RepoInfo> >( toModify, toModify.begin() ));
      for_( it, toModify.begin(), toModify.end() )
      {
        it->setService(service.name());
        modifyRepository(it->alias(), *it);
      }
    }

  }

  void RepoManager::Impl::knownServices()
  {
    ServiceCollector collector(services);
    Pathname dir = options.knownServicesPath;
    list<Pathname> entries;
    if (PathInfo(dir).isExist())
    {
      if ( filesystem::readdir( entries, Pathname(dir), false ) != 0 )
          ZYPP_THROW(Exception("failed to read directory"));

      //str::regex allowedServiceExt("^\\.service(_[0-9]+)?$");
      for_(it, entries.begin(), entries.end() )
      {
        parser::ServiceFileReader reader(*it,
            bind(&ServiceCollector::collect, collector, _1) );
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////

  std::ostream & operator<<( std::ostream & str, const RepoManager & obj )
  {
    return str << *obj._pimpl;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
