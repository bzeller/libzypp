/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/target/CommitPackageCache.cc
 *
*/
#include <iostream>
#include <zypp/base/Logger.h>
#include <zypp/base/Exception.h>
#include <zypp/ZConfig.h>
#include <zypp-core/base/UserRequestException>

#include <zypp-core/zyppng/base/EventDispatcher>
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-media/ng/Provide>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/zyppng/pipelines/Await>
#include <zypp-core/zyppng/pipelines/Wait>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/zyppng/pipelines/MTry>
#include <zypp-core/zyppng/meta/FunctionTraits>


#include <zypp/target/CommitPackageCache.h>
#include <zypp/target/CommitPackageCacheImpl.h>
#include <zypp/target/CommitPackageCacheReadAhead.h>

using std::endl;

#include <zypp/target/rpm/librpmDb.h>
#include <zypp/target/TargetException.h>
#include <zypp/repo/PackageProvider.h>
#include <zypp/repo/DeltaCandidates.h>
#include <zypp/ResPool.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace target
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace {
      ///////////////////////////////////////////////////////////////////
      /// \class QueryInstalledEditionHelper
      /// \short Helper for PackageProvider queries during download.
      ///////////////////////////////////////////////////////////////////
      struct QueryInstalledEditionHelper
      {
        bool operator()( const std::string & name_r, const Edition & ed_r, const Arch & arch_r ) const
        {
          rpm::librpmDb::db_const_iterator it;
          for ( it.findByName( name_r ); *it; ++it )
          {
            if ( arch_r == it->tag_arch()
              && ( ed_r == Edition::noedition || ed_r == it->tag_edition() ) )
            {
              return true;
            }
          }
          return false;
        }
      };
    } // namespace
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	class RepoProvidePackage
    //
    ///////////////////////////////////////////////////////////////////

    struct RepoProvidePackage::Impl
    {
      repo::RepoMediaAccess _access;
      std::list<Repository> _repos;
      repo::PackageProviderPolicy _packageProviderPolicy;
    };

    RepoProvidePackage::RepoProvidePackage()
      : _impl( new Impl )
    {
       const ResPool & pool( ResPool::instance() );
      _impl->_repos.insert( _impl->_repos.begin(), pool.knownRepositoriesBegin(), pool.knownRepositoriesEnd() );
      _impl->_packageProviderPolicy.queryInstalledCB( QueryInstalledEditionHelper() );
    }

    RepoProvidePackage::~RepoProvidePackage()
    {}

    ManagedFile RepoProvidePackage::operator()( const PoolItem & pi_r, bool fromCache_r )
    {
      ManagedFile ret;
      if ( fromCache_r )
      {
        repo::PackageProvider pkgProvider( _impl->_access, pi_r, _impl->_packageProviderPolicy );
        ret = pkgProvider.providePackageFromCache();
      }
      else if ( pi_r.isKind<Package>() )	// may make use of deltas
      {
        repo::DeltaCandidates deltas( _impl->_repos, pi_r.name() );
        repo::PackageProvider pkgProvider( _impl->_access, pi_r, deltas, _impl->_packageProviderPolicy );
        return pkgProvider.providePackage();
      }
      else	// SrcPackage or throws
      {
        repo::PackageProvider pkgProvider( _impl->_access, pi_r, _impl->_packageProviderPolicy );
        return pkgProvider.providePackage();
      }
      return ret;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : CommitPackageCache
    //
    ///////////////////////////////////////////////////////////////////

    CommitPackageCache::CommitPackageCache( Impl * pimpl_r )
    : _pimpl( pimpl_r )
    {
      assert( _pimpl );
    }

    CommitPackageCache::CommitPackageCache( const PackageProvider & packageProvider_r )
    {
      if ( getenv("ZYPP_COMMIT_NO_PACKAGE_CACHE") )
        {
          MIL << "$ZYPP_COMMIT_NO_PACKAGE_CACHE is set." << endl;
          _pimpl.reset( new Impl( packageProvider_r ) ); // no cache
        }
      else
        {
          _pimpl.reset( new CommitPackageCacheReadAhead( packageProvider_r ) );
        }
      assert( _pimpl );
    }

    CommitPackageCache::CommitPackageCache( const Pathname &        /*rootDir_r*/,
                                            const PackageProvider & packageProvider_r )
    : CommitPackageCache( packageProvider_r )
    {}

   CommitPackageCache::~CommitPackageCache()
    {}

    void CommitPackageCache::setCommitList( std::vector<sat::Solvable> commitList_r )
    { _pimpl->setCommitList( commitList_r ); }


    bool CommitPackageCache::preloadForTransaction ( std::vector<sat::Transaction::Step> &steps )
    {
        //need to pull in the overloaded operators explicitely
        using zyppng::operators::mbind;
        using zyppng::operators::operator|;

        bool miss = false;
        bool doAsyncPreload = true;

        if ( doAsyncPreload ) {

          // this code is highly experimental and temporary, starting the event loop here should not be the default.
          // instead the pipeline should be built up into the application code ( zypper ) and should be controlled from there
          // however until we get to a point where we can do that we need to do this dirty hack
          // Be careful when mixing this with a EventLoop that is in a higher level than the one we start here
          // firing events will trigger events for both loop instances
          auto loop = zyppng::EventLoop::create();

          // out provider, this will drive the pipelines to finish
          auto prov = zyppng::Provide::create();

          const ResPool & pool( ResPool::instance() );

          std::list<Repository> allRepos;
          allRepos.insert( allRepos.begin(), pool.knownRepositoriesBegin(), pool.knownRepositoriesEnd() );

          const auto & asyncPackageProvider = [ &allRepos, &prov ]( PoolItem &&pi ) -> zyppng::AsyncOpRef<zyppng::expected<ManagedFile>> {

            // first step is top try and provide it directly from cache
            if ( pi.isKind<Package>() )	// may make use of deltas
            {
              auto package = pi->asKind<Package>();
              if ( !package->cachedLocation().empty() ) {
                return zyppng::makeReadyResult<zyppng::expected<ManagedFile>>( zyppng::expected<ManagedFile>::success( ManagedFile( package->cachedLocation() ) ) );
              }

              // @TODO check toplevel cache

              repo::PackageProviderPolicy policy;  // << make this a arg
              policy.queryInstalledCB( QueryInstalledEditionHelper() );

              repo::DeltaCandidates deltas( allRepos, pi.name() );
              return std::move(pi) | repo::AsyncPackageProvider::provide( prov, deltas, policy );
            }
            else	// SrcPackage or throws
            {
              //repo::PackageProvider pkgProvider( _impl->_access, pi_r, _impl->_packageProviderPolicy );
              //return pkgProvider.providePackage();
            }


          };

          std::vector< sat::Transaction::Step * > downloadSteps;
          for ( auto &step : steps ) {
            PoolItem pi( step );
            if ( pi->isKind<Package>() || pi->isKind<SrcPackage>() ) {
              downloadSteps.push_back( &step );
            }
          }

          auto res = zyppng::transform( std::move(downloadSteps), [&]( sat::Transaction::Step *s ){
            return asyncPackageProvider( PoolItem( *s ));
          })
          | zyppng::waitFor<zyppng::expected<zypp::ManagedFile>>()
          | [&]( std::vector<zyppng::expected<zypp::ManagedFile>> &&res ) {
            loop->quit();
            return res;
          };

          loop->run();
          if ( !res->isReady() ) {
            // impossible , this is a bug
            WAR << "Running the loop failed" << endl;
            return false;
          }

          for ( auto &r : res->get() ) {
            if ( !r ) {
              miss = true;
            } else {
              r->resetDispose(); // keep the package file in the cache

            }
          }

        } else {

          for_( it, steps.begin(), steps.end() )
          {
            switch ( it->stepType() )
            {
              case sat::Transaction::TRANSACTION_INSTALL:
              case sat::Transaction::TRANSACTION_MULTIINSTALL:
                // proceed: only install actionas may require download.
                break;

              default:
                // next: no download for or non-packages and delete actions.
                continue;
                break;
            }

            PoolItem pi( *it );
            if ( pi->isKind<Package>() || pi->isKind<SrcPackage>() )
            {
              ManagedFile localfile;
              try {
                localfile = get( pi );
                localfile.resetDispose(); // keep the package file in the cache

              } catch ( const AbortRequestException & exp ) {
                it->stepStage( sat::Transaction::STEP_ERROR );
                miss = true;
                WAR << "commit cache preload aborted by the user" << endl;
                ZYPP_THROW( TargetAbortedException( ) );
                break;

              } catch ( const SkipRequestException & exp ) {
                ZYPP_CAUGHT( exp );
                it->stepStage( sat::Transaction::STEP_ERROR );
                miss = true;
                WAR << "Skipping cache preload package " << pi->asKind<Package>() << " in commit" << endl;
                continue;

              } catch ( const Exception & exp ) {
                // bnc #395704: missing catch causes abort.
                // TODO see if packageCache fails to handle errors correctly.
                ZYPP_CAUGHT( exp );
                it->stepStage( sat::Transaction::STEP_ERROR );
                miss = true;
                INT << "Unexpected Error: Skipping cache preload package " << pi->asKind<Package>() << " in commit" << endl;
                continue;

              }
            }
          }

        }
        preloaded( true ); // try to avoid duplicate infoInCache CBs in commit
        return (!miss);
    }

    ManagedFile CommitPackageCache::get( const PoolItem & citem_r )
    { return _pimpl->get( citem_r ); }

    bool CommitPackageCache::preloaded() const
    { return _pimpl->preloaded(); }

    void CommitPackageCache::preloaded( bool newval_r )
    { _pimpl->preloaded( newval_r ); }

    /******************************************************************
    **
    **	FUNCTION NAME : operator<<
    **	FUNCTION TYPE : std::ostream &
    */
    std::ostream & operator<<( std::ostream & str, const CommitPackageCache & obj )
    { return str << *obj._pimpl; }

    /////////////////////////////////////////////////////////////////
  } // namespace target
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
