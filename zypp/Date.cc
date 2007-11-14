/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Date.cc
 *
*/
#include <iostream>
//#include "zypp/base/Logger.h"

#include "zypp/base/String.h"

#include "zypp/Date.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Date::Date
  //	METHOD TYPE : Constructor
  //
  Date::Date( const std::string & seconds_r )
  { str::strtonum( seconds_r, _date ); }

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Date::form
  //	METHOD TYPE : std::string
  //
  std::string Date::form( const std::string & format_r ) const
  {
    static char buf[1024];

    static std::string lastLocale;
    static std::string needLocale;
    const char * tmp = ::setlocale( LC_TIME, NULL );
    std::string thisLocale( tmp ? tmp : "" );

    if ( thisLocale != lastLocale )
    {
      // locale changes since last call. compute new utf-8 locale
      if (    thisLocale.find( "UTF-8" ) != std::string::npos
           || thisLocale.find( "utf-8" ) != std::string::npos
           || thisLocale == "POSIX"
           || thisLocale == "C"
           || thisLocale == "" )
      {
        // is UTF-8 or C or POSIX
        needLocale = thisLocale;
      }
      else
      {
        // language[_territory][.codeset][@modifier]
        // add/exchange codeset with UTF-8
        needLocale = ".UTF-8";
        std::string::size_type loc = thisLocale.find_first_of( ".@" );
        if ( loc != std::string::npos )
        {
          // prepend language[_territory]
          needLocale = thisLocale.substr( 0, loc ) + needLocale;
          loc = thisLocale.find_last_of( "@" );
          if ( loc != std::string::npos )
          {
            // append [@modifier]
            needLocale += thisLocale.substr( loc );
          }
        }
        else
        {
          // append ".UTF-8"
          needLocale = thisLocale + needLocale;
        }
      }
      lastLocale = thisLocale;
    }

    if ( needLocale != lastLocale )
      ::setlocale( LC_TIME, needLocale.c_str() );
    if ( ! strftime( buf, 1024, format_r.c_str(), localtime( &_date ) ) )
      *buf = '\0';
    if ( needLocale != lastLocale )
      ::setlocale( LC_TIME, lastLocale.c_str() );
    return buf;
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
