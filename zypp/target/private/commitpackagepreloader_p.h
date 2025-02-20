/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_TARGET_PRIVATE_COMMITPACKAGEPRELOADER_H
#define ZYPP_TARGET_PRIVATE_COMMITPACKAGEPRELOADER_H

#include <zypp-core/Pathname.h>
#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp/sat/Transaction.h>
#include <zypp/media/UrlResolverPlugin.h>

#include <map>
#include <deque>

namespace zyppng {
ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequestDispatcher);
ZYPP_FWD_DECL_TYPE_WITH_REFS(NetworkRequest);
}

namespace zypp {

class CommitPackagePreloader
{
public:
  CommitPackagePreloader( zypp::Pathname cacheDir );

  void preloadTransaction( std::vector<sat::Transaction::Step> steps );

private:
  class PreloadWorker;
  struct RepoUrl {
    zypp::Url baseUrl;
    media::UrlResolverPlugin::HeaderList headers;
  };

  struct RepoDownloadData {
    std::vector<RepoUrl> _baseUrls;
  };

  std::map<Repository::IdType, RepoDownloadData> _dlRepoInfo;
  std::deque<PoolItem> _requiredDls;

  zypp::Pathname _destDir;
  zyppng::NetworkRequestDispatcherRef _dispatcher;
};

}

#endif // ZYPP_TARGET_PRIVATE_COMMITPACKAGEPRELOADER_H
