/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ng/parser/RepoFileReader.h
 *
*/
#ifndef ZYPP_REPO_REPOFILEREADER_H
#define ZYPP_REPO_REPOFILEREADER_H

#include <iosfwd>

#include <zypp/base/PtrTypes.h>
#include <zypp-core/base/InputStream>
#include <zypp/RepoInfo.h>
#include <zypp-core/ui/ProgressData>
#include <zypp/ng/contextbase.h>

///////////////////////////////////////////////////////////////////
namespace zyppng
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace parser
  { /////////////////////////////////////////////////////////////////

    /**
     * \short Read repository data from a .repo file
     *
     * After each repo is read, a \ref RepoInfo is prepared and \ref _callback
     * is called with the object passed in.
     *
     * The \ref _callback is provided on construction.
     *
     * \code
     * RepoFileReader reader(repo_file,
     *                bind( &SomeClass::callbackfunc, &SomeClassInstance, _1, _2 ) );
     * \endcode
     *
     * \note Multiple baseurls in a repo file are supported using this style:
     * \code
     * baseurl=http://server.a/path/to/repo
     *         http://server.b/path/to/repo
     *         http://server.c/path/to/repo
     * \endcode
     * Repeating the \c baseurl= tag on each line is also accepted, but when the
     * file has to be written, the preferred style is used.
     */
    class RepoFileReader
    {
      friend std::ostream & operator<<( std::ostream & str, const RepoFileReader & obj );
    public:

     /**
      * Callback definition.
      * First parameter is a \ref RepoInfo object with the resource
      * second parameter is the resource type.
      *
      * Return false from the callback to get a \ref AbortRequestException
      * to be thrown and the processing to be cancelled.
      */
      using ProcessRepo = std::function<bool (const RepoInfo &)>;

      /** Implementation  */
      class Impl;

    public:
     /**
      * \short Constructor. Creates the reader and start reading.
      *
      * \param context The zypp context this RepoFileReader should evaluate in
      * \param repo_file A valid .repo file
      * \param callback Callback that will be called for each repository.
      * \param progress Optional progress function. \see ProgressData
      *
      * \throws AbortRequestException If the callback returns false
      * \throws Exception If a error occurs at reading / parsing
      *
      */
      RepoFileReader( ContextBaseRef context,
                      const zypp::Pathname & repo_file,
                      ProcessRepo  callback,
                      const zypp::ProgressData::ReceiverFnc &progress = zypp::ProgressData::ReceiverFnc() );

     /**
      * \short Constructor. Creates the reader and start reading.
      *
      * \param is A valid input stream
      * \param callback Callback that will be called for each repository.
      * \param progress Optional progress function. \see ProgressData
      *
      * \throws AbortRequestException If the callback returns false
      * \throws Exception If a error occurs at reading / parsing
      *
      */
      RepoFileReader( ContextBaseRef context,
                      const zypp::InputStream &is,
                      ProcessRepo  callback,
                      const zypp::ProgressData::ReceiverFnc &progress = zypp::ProgressData::ReceiverFnc() );

      /**
       * Dtor
       */
      ~RepoFileReader();
    private:
      ContextBaseRef _context;
      ProcessRepo _callback;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates RepoFileReader Stream output */
    std::ostream & operator<<( std::ostream & str, const RepoFileReader & obj );

    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_REPO_REPOFILEREADER_H