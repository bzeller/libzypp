/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_LOGIC_DEFINES_INCLUDED
#define ZYPP_LOGIC_DEFINES_INCLUDED

#include <zypp-core/Globals.h>
#include <zypp-core/zyppng/base/zyppglobal.h>

#ifdef ZYPP_USE_CORO

#define zypp_co_await  co_await
#define zypp_co_return co_return

namespace zyppng {
  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1 ( AsyncOpRef, T );
  template<class Type>
  using MaybeAwaitable = AsyncOpRef<Type>;
}

#else

#define zypp_co_await
#define zypp_co_return return

namespace zyppng {
  template<class Type>
  using MaybeAwaitable = Type;
}

#endif



#endif
