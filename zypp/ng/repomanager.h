/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPOMANAGER_INCLUDED
#define ZYPP_NG_REPOMANAGER_INCLUDED

#include <zypp-core/base/Iterable.h>
#include <zypp-core/zyppng/base/base.h>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-core/zyppng/async/asyncop.h>
#include <zypp/RepoManager.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( RepoManager );
  class RepoManagerPrivate;

  using RepoManagerOptions = zypp::RepoManagerOptions;
  using RepoInfo = zypp::RepoInfo;

  /*!
   * The RepoManager class is the central place when it comes
   * to handling all repo related functionality. It directly belongs to a \ref Context
   * and can be used to query and manipulate which repositories are used.
   */
  class RepoManager : public Base
  {
    ZYPP_DECLARE_PRIVATE(RepoManager)
    ZYPP_ADD_CREATE_FUNC(RepoManager)

  public:

    using RefreshCheckStatus = zypp::RepoManager::RefreshCheckStatus;
    using RawMetadataRefreshPolicy = zypp::RepoManager::RawMetadataRefreshPolicy;

    /** ServiceInfo typedefs */
    using ServiceSet = zypp::RepoManager::ServiceSet;
    using ServiceConstIterator = zypp::RepoManager::ServiceConstIterator;
    using ServiceSizeType = zypp::RepoManager::ServiceSizeType;

    /** RepoInfo typedefs */
    using RepoSet = zypp::RepoManager::RepoSet;
    using RepoConstIterator = zypp::RepoManager::RepoConstIterator;
    using RepoSizeType = zypp::RepoManager::RepoSizeType;

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(RepoManager, ContextRef context, const RepoManagerOptions &options = RepoManagerOptions() );

    /** \name Known repositories.
     *
     * The known repositories are read from
     * \ref RepoManagerOptions::knownReposPath passed on the Ctor.
     * Which defaults to ZYpp global settings.
     */
    //@{
    bool repoEmpty() const;
    RepoSizeType repoSize() const;
    RepoConstIterator repoBegin() const;
    RepoConstIterator repoEnd() const;
    zypp::Iterable<RepoConstIterator> repos() const;

    AsyncOpRef<expected<RefreshCheckStatus>> checkIfToRefreshMetadata( const RepoInfo &info,
                                                      const zypp::Url &url,
                                                      RawMetadataRefreshPolicy policy = zypp::RepoManager::RefreshIfNeeded);

    /** List of known repositories. */
    std::list<RepoInfo> knownRepositories() const
    { return std::list<RepoInfo>(repoBegin(),repoEnd()); }

  };

}



#endif
