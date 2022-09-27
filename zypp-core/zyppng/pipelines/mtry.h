/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
* Based on code by Ivan Čukić (BSD/MIT licensed) from the functional cpp book
*/

#ifndef ZYPP_ZYPPNG_MONADIC_MTRY_H
#define ZYPP_ZYPPNG_MONADIC_MTRY_H

#include "expected.h"

namespace zyppng {

  namespace detail {

    template  <typename Callback>
    struct MtryImpl {

      MtryImpl( Callback &&cb ) : _cb( std::move(cb)) { }
      MtryImpl( const Callback &cb ) : _cb( cb ) { }

      template <typename Msg, typename Ret = std::invoke_result_t<Callback, Msg> >
      zyppng::expected<Ret> operator() ( Msg &&arg ) {
        try {
            return zyppng::expected<Ret>::success( std::invoke( _cb, std::forward<Msg>(arg) ) );
        } catch (...) {
            return zyppng::expected<Ret>::error(std::current_exception());
        }
      }
      private:
        Callback _cb;
    };
  }

  template < typename F >
  detail::MtryImpl<F> mtry(F &&f) {
    return detail::MtryImpl( std::forward<F>(f));
  }

}

#endif /* !MTRY_H */
