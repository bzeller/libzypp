#ifndef ZYPPNG_MEDIA_PATHREF_P_H_INCLUDED
#define ZYPPNG_MEDIA_PATHREF_P_H_INCLUDED

#include <zypp/zyppng/media/PathRef>
#include <zypp/ManagedFile.h>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/base/NonCopyable.h>

namespace zyppng {

  class PathRefPrivate : public trackable, public zypp::base::NonCopyable
  {
    public:
      PathRefPrivate ();
      PathRefPrivate ( std::shared_ptr<DeviceHandler> &&ref, zypp::Pathname path );
      PathRefPrivate ( zypp::Pathname path );
      ~PathRefPrivate ();
      void init ();
      void handlerShutdown ();
      void reset ();

      connection _handlerConnection;
      std::shared_ptr<DeviceHandler> _handler;
      zypp::ManagedFile _path;
      Signal<void()> _released;
  };
}



#endif //  ZYPPNG_MEDIA_PATHREF_P_H_INCLUDED
