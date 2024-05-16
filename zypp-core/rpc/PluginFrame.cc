/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/PluginFrame.cc
 *
*/
#include <iostream>
#include <utility>
#include <zypp-core/base/LogTools.h>
#include <zypp-core/rpc/PluginFrame.h>
#include <zypp-core/zyppng/core/String>

using std::endl;

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::plugin"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  constexpr static std::string_view ContentLenHdr("content-length");

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : PluginFrame::Impl
  //
  /** PluginFrame implementation. */
  struct PluginFrame::Impl
  {
    public:
      Impl()
      {}

      Impl( const std::string & command_r )
      { setCommand( command_r ); }

      Impl( const std::string & command_r, ByteArray &&body_r )
        : _body(std::move( body_r ))
      { setCommand( command_r ); }

      Impl( const std::string & command_r, HeaderInitializerList contents_r )
      { setCommand( command_r ); addHeader( contents_r ); }

      Impl( const std::string & command_r, ByteArray &&body_r, HeaderInitializerList contents_r )
        : _body(std::move( body_r ))
      { setCommand( command_r ); addHeader( contents_r ); }

      Impl( std::istream & stream_r );

    public:
      bool empty() const
      { return _command.empty() && _body.empty(); }

      const std::string & command() const
      { return _command; }

      void setCommand( const std::string & command_r )
      {
        if ( command_r.find( '\n' ) != std::string::npos )
          ZYPP_THROW( PluginFrameException( "Multiline command", command_r ) );
        _command = command_r;
      }

      const ByteArray & body() const
      { return _body; }

      ByteArray & bodyRef()
      { return _body; }

      void setBody( const ByteArray & body_r )
      { _body = body_r; }

    public:
      using constKeyRange = std::pair<HeaderListIterator, HeaderListIterator>;
      using KeyRange = std::pair<HeaderList::iterator, HeaderList::iterator>;

      HeaderList & headerList()
      { return _header; }

      const HeaderList & headerList() const
      { return _header; }

      const std::string & getHeader( const std::string & key_r ) const
      {
        constKeyRange r( _header.equal_range( key_r ) );
        if ( r.first == r.second )
          ZYPP_THROW( PluginFrameException( "No value for key", key_r ) );
        const std::string & ret( r.first->second );
        if ( ++r.first != r.second )
          ZYPP_THROW( PluginFrameException( "Multiple values for key", key_r ) );
        return ret;
      }

      const std::string & getHeader( const std::string & key_r, const std::string & default_r ) const
      {
        constKeyRange r( _header.equal_range( key_r ) );
        if ( r.first == r.second )
          return default_r;
        const std::string & ret( r.first->second );
        if ( ++r.first != r.second )
          ZYPP_THROW( PluginFrameException( "Multiple values for key", key_r ) );
        return ret;
      }

      const std::string & getHeaderNT( const std::string & key_r, const std::string & default_r ) const
      {
        HeaderListIterator iter( _header.find( key_r ) );
        return iter != _header.end() ? iter->second : default_r;
      }

      HeaderList::value_type mkHeaderPair( const std::string & key_r, const std::string & value_r )
      {
        if ( key_r.find_first_of( ":\n" ) != std::string::npos )
          ZYPP_THROW( PluginFrameException( "Illegal char in header key", key_r ) );
        if ( value_r.find_first_of( '\n' ) != std::string::npos )
          ZYPP_THROW( PluginFrameException( "Illegal char in header value", value_r ) );
        return HeaderList::value_type( key_r, value_r );
      }

      void setHeader( const std::string & key_r, const std::string & value_r )
      {
        clearHeader( key_r );
        addHeader( key_r, value_r );
      }

      void addHeader( const std::string & key_r, const std::string & value_r )
      {
        _header.insert( mkHeaderPair( key_r, value_r ) );
      }

      void addHeader( HeaderInitializerList contents_r )
      {
        for ( const auto & el : contents_r )
          addHeader( el.first, el.second );
      }

      void clearHeader( const std::string & key_r )
      {
        _header.erase( key_r );
      }

    public:
      std::ostream & writeTo( std::ostream & stream_r ) const;

    private:
      std::string _command;
      ByteArray   _body;
      HeaderList  _header;

    public:
      /** Offer default Impl. */
      static shared_ptr<Impl> nullimpl()
      {
        static shared_ptr<Impl> _nullimpl( new Impl );
        return _nullimpl;
      }
    private:
      friend Impl * rwcowClone<Impl>( const Impl * rhs );
      /** clone for RWCOW_pointer */
      Impl * clone() const
      { return new Impl( *this ); }
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates PluginFrame::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const PluginFrame::Impl & obj )
  {
    return str << "PluginFrame[" << obj.command() << "](" << obj.headerList().size() << "){" << obj.body().size() << "}";
  }

  PluginFrame::Impl::Impl( std::istream & stream_r )
  {
    //DBG << "Parse from " << stream_r << endl;
    if ( ! stream_r )
      ZYPP_THROW( PluginFrameException( "Bad Stream" ) );

    // JFYI: stream status after getline():
    //  Bool  | Bits
    //  ------|---------------
    //  true  | [g___] >FOO< : FOO line was \n-terminated
    //  true  | [_e__] >BAA< : BAA before EOF, but not \n-terminated
    //  false | [_eF_] ><    : No valid data to consume

    //command
    _command = str::getline( stream_r );
    if ( ! stream_r.good() )
      ZYPP_THROW( PluginFrameException( "Missing NL after command" ) );

    // header
    do {
      std::string data = str::getline( stream_r );
      if ( ! stream_r.good() )
        ZYPP_THROW( PluginFrameException( "Missing NL after header" ) );

      if ( data.empty() )
        break;	// --> empty line sep. header and body

      std::string::size_type sep( data.find( ':') );
      if ( sep ==  std::string::npos )
        ZYPP_THROW( PluginFrameException( "Missing colon in header" ) );

      _header.insert( HeaderList::value_type( data.substr(0,sep), data.substr(sep+1) ) );
    } while ( true );


    const auto &contentLen = getHeaderNT( std::string{ContentLenHdr}, std::string() );
    std::optional<uint64_t> cLen;
    if ( !contentLen.empty() ) {
      cLen = zyppng::str::safe_strtonum<uint64_t>(contentLen);
      if ( !cLen ) {
        ERR << "Invalid value for " << ContentLenHdr << ":" << contentLen << std::endl;
        ZYPP_THROW( PluginFrameException( "Invalid value for content-len" ) );
      }
    }

    if ( cLen ) {
      _body.reserve ( *cLen );

    } else {
      // data
      const auto &data = str::receiveUpTo( stream_r, '\0' );
      _body = ByteArray( data.c_str(), data.size() );
      if ( ! stream_r.good() )
        ZYPP_THROW( PluginFrameException( "Missing NUL after body" ) );
    }
  }

  std::ostream & PluginFrame::Impl::writeTo( std::ostream & stream_r ) const
  {
    //DBG << "Write " << *this << " to " << stream_r << endl;
    if ( ! stream_r )
      ZYPP_THROW( PluginFrameException( "Bad Stream" ) );

    // command
    stream_r << _command << "\n";

    // if the body contains a \0 we need to send the content-length header
    if ( std::find( _body.begin(), _body.end(), '\0') != _body.end() ) {
      stream_r << ContentLenHdr << ':' << str::numstring( _body.size() ) << "\n";
    }

    // header
    for_( it, _header.begin(), _header.end() )
      stream_r << it->first << ':' << it->second << "\n";

    // header end
    stream_r << "\n";

    // body
    stream_r.write( _body.data(), _body.size() );

    // body end
    stream_r << '\0';
    stream_r.flush();

    if ( ! stream_r )
      ZYPP_THROW( PluginFrameException( "Write error" ) );
    return stream_r;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : PluginFrame
  //
  ///////////////////////////////////////////////////////////////////

  const std::string & PluginFrame::ackCommand()
  {
    static std::string _val( "ACK" );
    return _val;
  }

  const std::string & PluginFrame::errorCommand()
  {
    static std::string _val( "ERROR" );
    return _val;
  }

  const std::string & PluginFrame::enomethodCommand()
  {
    static std::string _val( "_ENOMETHOD" );
    return _val;
  }

  PluginFrame::PluginFrame()
    : _pimpl( Impl::nullimpl() )
  {}

  PluginFrame::PluginFrame( const std::string & command_r )
    : _pimpl( new Impl( command_r ) )
  {}

  PluginFrame::PluginFrame(const std::string & command_r, std::string body_r )
    : _pimpl( new Impl( command_r, ByteArray(body_r) ) )
  {}

  PluginFrame::PluginFrame(const std::string & command_r, ByteArray body_r )
    : _pimpl( new Impl( command_r, std::move(body_r) ) )
  {}

  PluginFrame::PluginFrame( const std::string & command_r, HeaderInitializerList contents_r )
    : _pimpl( new Impl( command_r, contents_r ) )
  {}

  PluginFrame::PluginFrame(const std::string & command_r, ByteArray body_r, HeaderInitializerList contents_r )
    : _pimpl( new Impl( command_r, std::move(body_r), contents_r ) )
  {}

  PluginFrame::PluginFrame( std::istream & stream_r )
    : _pimpl( new Impl( stream_r ) )
  {}

  bool PluginFrame::empty() const
  { return _pimpl->empty(); }

  const std::string & PluginFrame::command() const
  { return _pimpl->command(); }

  void  PluginFrame::setCommand( const std::string & command_r )
  { _pimpl->setCommand( command_r ); }

  const ByteArray &PluginFrame::body() const
  { return _pimpl->body(); }

  ByteArray &PluginFrame::bodyRef()
  { return _pimpl->bodyRef(); }

  void  PluginFrame::setBody( const std::string & body_r )
  { _pimpl->setBody( ByteArray(body_r.data(), body_r.size()) ); }

  std::ostream & PluginFrame::writeTo( std::ostream & stream_r ) const
  { return _pimpl->writeTo( stream_r ); }

  PluginFrame::HeaderList & PluginFrame::headerList()
  { return _pimpl->headerList(); }

  const PluginFrame::HeaderList & PluginFrame::headerList() const
  { return _pimpl->headerList(); }

  const std::string & PluginFrame::getHeader( const std::string & key_r ) const
  { return _pimpl->getHeader( key_r ); }

  const std::string & PluginFrame::getHeader( const std::string & key_r, const std::string & default_r ) const
  { return _pimpl->getHeader( key_r, default_r ); }

  const std::string & PluginFrame::getHeaderNT( const std::string & key_r, const std::string & default_r ) const
  { return _pimpl->getHeaderNT( key_r, default_r ); }

  void PluginFrame::setHeader( const std::string & key_r, const std::string & value_r )
  { _pimpl->setHeader( key_r, value_r ); }

  void PluginFrame::addHeader( const std::string & key_r, const std::string & value_r )
  { _pimpl->addHeader( key_r, value_r ); }

  void PluginFrame::addHeader( HeaderInitializerList contents_r )
  { _pimpl->addHeader( contents_r ); }

  void PluginFrame::clearHeader( const std::string & key_r )
  { _pimpl->clearHeader( key_r ); }

  ///////////////////////////////////////////////////////////////////

  std::ostream & operator<<( std::ostream & str, const PluginFrame & obj )
  { return str << *obj._pimpl; }

  bool operator==( const PluginFrame & lhs, const PluginFrame & rhs )
  {
    return ( lhs._pimpl == rhs._pimpl )
        || (( lhs.command() ==  rhs.command() ) && ( lhs.headerList() == rhs.headerList() ) && ( lhs.body() == rhs.body() ));
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
