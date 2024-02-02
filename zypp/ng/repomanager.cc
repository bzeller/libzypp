/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/repomanager_p.h"
#include <zypp/ng/Context>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/ng/repo/workflows/repomanagerwf.h>

namespace zyppng {

  ZYPP_IMPL_PRIVATE (RepoManager)

  RepoManagerPrivate::RepoManagerPrivate(ContextRef ctx, RepoManagerOptions repoOpts, RepoManager &p)
    : BasePrivate(p)
    , zypp::RepoManagerBaseImpl( repoOpts )
    , _context(ctx)
  {
    init_knownServices();
    init_knownRepositories();
  }

  AsyncOpRef<expected<zypp::repo::RepoType> > RepoManagerPrivate::probe(const zypp::Url &url, const zypp::filesystem::Pathname &path) const
  {
    return makeReadyResult( expected<zypp::repo::RepoType>::error( std::make_exception_ptr(zypp::Exception("Not implemented")) ) );
  }

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS( RepoManager, ContextRef context, const RepoManagerOptions &options ) : Base( *( new RepoManagerPrivate( context, options, *this) ))
  {

  }

  bool RepoManager::repoEmpty() const
  { return d_func()->repoEmpty(); }

  RepoManager::RepoSizeType RepoManager::repoSize() const
  { return d_func()->repoSize(); }

  RepoManager::RepoConstIterator RepoManager::repoBegin() const
  { return d_func()->repoBegin(); }

  RepoManager::RepoConstIterator RepoManager::repoEnd() const
  { return d_func()->repoEnd(); }

  /** Iterate the known repositories. */
  inline zypp::Iterable<RepoManager::RepoConstIterator> RepoManager::repos() const
  { return zypp::makeIterable( repoBegin(), repoEnd() ); }


  AsyncOpRef<expected<RepoManager::RefreshCheckStatus> > RepoManager::checkIfToRefreshMetadata( const RepoInfo &info, const zypp::Url &url, RawMetadataRefreshPolicy policy )
  {
    Z_D();

    using namespace zyppng::operators;

    auto zyppContext = d->_context.lock();
    if ( !zyppContext )
      return makeReadyResult( expected<RepoManager::RefreshCheckStatus>::error( ZYPP_EXCPT_PTR (zypp::Exception("Invalid Zypp context"))) );

#if 0
    // implement attach media stuff before
    zyppContext->provider()->attachMedia( info.baseUrls(), ProvideMediaSpec() );

    return repo::AsyncRefreshContext::create ( zyppContext, info, zypp::rawcache_path_for_repoinfo( d->_options, info ) )
        | and_then([]( repo::AsyncRefreshContextRef &&refCtx ) {
      return RepoManagerWorkflow::checkIfToRefreshMetadata( refCtx )
    });
#endif
    return makeReadyResult( expected<RepoManager::RefreshCheckStatus>::error( ZYPP_EXCPT_PTR (zypp::Exception("Implementme"))) );
  }

  void zyppng::RepoManagerPrivate::removeRepository( const RepoInfo &info, const zypp::ProgressData::ReceiverFnc &recfnc )
  {
    removeRepositoryImpl( info, recfnc );
  }

}
