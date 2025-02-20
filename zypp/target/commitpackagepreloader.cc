#include "private/commitpackagepreloader_p.h"
#include <zypp/media/MediaHandlerFactory.h> // to detect the URL type
#include <zypp-media/mediaconfig.h>
#include <zypp/media/UrlResolverPlugin.h>
#include <zypp-core/zyppng/base/eventloop.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-curl/transfersettings.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp/MediaSetAccess.h>

namespace zypp {

struct RepoUrl {
  zypp::Url baseUrl;
  media::UrlResolverPlugin::HeaderList headers;
};

class CommitPackagePreloader::PreloadWorker {
public:
  enum State {
    Pending,
    SimpleDl,
    ZckHead,
    ZckData,
    Finished
  };

  PreloadWorker( CommitPackagePreloader &parent ) : _parent(parent) {}

  auto getMirror( const PoolItem &pi ) {
    auto &repoDlInfo = _parent._dlRepoInfo.at( pi.repository().id() );
    return repoDlInfo._baseUrls.begin();
  }

  void nextJob () {

    auto job = std::move(_parent._requiredDls.front());
    _parent._requiredDls.pop_front();

    auto &repoDlInfo = _parent._dlRepoInfo.at( job.repository().id() );
    auto loc = job.lookupLocation();


    if ( _s == Pending ) {
      // init case, set up request
      zypp::Url url = repoDlInfo._baseUrls.front().baseUrl;
      _req = std::make_shared<zyppng::NetworkRequest>(  );
    } else {

    }

  }

private:
  void onRequestStarted ( zyppng::NetworkRequest &req ) {

  }

  void onRequestFinished( zyppng::NetworkRequest &req ) {

  }

  void onRequestProgress( zyppng::NetworkRequest &req ) {

  }

private:
  State _s = Pending;
  CommitPackagePreloader &_parent;
  zyppng::NetworkRequestRef _req;

};

CommitPackagePreloader::CommitPackagePreloader( Pathname cacheDir )
  :_destDir( std::move(cacheDir) )
{}

void CommitPackagePreloader::preloadTransaction(std::vector<sat::Transaction::Step> steps)
{

  auto ev = zyppng::EventLoop::create();
  _dispatcher = std::make_shared<zyppng::NetworkRequestDispatcher>();
  _dispatcher->setMaximumConcurrentConnections( MediaConfig::instance().download_max_concurrent_connections() );

  for ( const auto &step : steps ) {
    switch ( step.stepType() )
    {
      case sat::Transaction::TRANSACTION_INSTALL:
      case sat::Transaction::TRANSACTION_MULTIINSTALL:
        // proceed: only install actions may require download.
        break;

      default:
        // next: no download for non-packages and delete actions.
        continue;
        break;
    }

    PoolItem pi(step.satSolvable());

    if ( !pi->isKind<Package>() && !pi->isKind<SrcPackage>() )
      continue;

    auto repoDlsIter = _dlRepoInfo.find( pi.repository().id() );
    if ( repoDlsIter == _dlRepoInfo.end() ) {
      // filter base URLs that do not download
      std::vector<RepoUrl> repoUrls;
      const auto bu = pi.repoInfo().baseUrls();
      std::for_each( bu.begin(), bu.end(), [&]( const zypp::Url &u ) {
        media::UrlResolverPlugin::HeaderList custom_headers;
        Url url = media::UrlResolverPlugin::resolveUrl(u, custom_headers);
        MIL << "Trying scheme '" << url.getScheme() << "'" << std::endl;

        if ( media::MediaHandlerFactory::handlerType(url) != media::MediaHandlerFactory::MediaCURLType )
          return;

        // TODO here we could block to fetch mirror informations, either if the RepoInfo has a metalink or mirrorlist entry
        // or if the hostname of the repo is d.o.o

        repoUrls.push_back( RepoUrl {
          .baseUrl = std::move(url),
          .headers = std::move(custom_headers)
        } );
      });

      // skip this solvable if it has no downloading base URLs
      if( repoUrls.empty() ) {
        MIL << "Skipping predownload for " << step.satSolvable() << " no downloading URL" << std::endl;
        continue;
      }

      _dlRepoInfo.insert( std::make_pair(
        pi.repository().id(),
        RepoDownloadData{
          ._baseUrls = std::move(repoUrls)
        }
      ));
    }
    _requiredDls.push_back( pi );
  }

  if ( _requiredDls.empty() )
    return;

  // order by repo
  std::sort( _requiredDls.begin(), _requiredDls.end(), []( const PoolItem &a , const PoolItem &b ) { return a.repository() < b.repository(); });

  // check cache if file is there already
  // -> no checksum avail -> IFMODSINCE
}

}
