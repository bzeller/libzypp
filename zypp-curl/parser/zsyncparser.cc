/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/ZsyncParser.cc
 *
 */

#include "zsyncparser.h"
#include <zypp-core/base/Logger.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>
#include <iostream>
#include <fstream>

using std::endl;
using namespace zypp::base;

namespace zypp::env
{
  /** Hack to circumvent the currently poor --root support. */
  inline bool ZYPP_METALINK_DEBUG()
  {
    static bool val = [](){
      const char * env = getenv("ZYPP_METALINK_DEBUG");
      return( env && zypp::str::strToBool( env, true ) );
    }();
    return val;
  }
}

namespace zypp {
  namespace media {

ZsyncParser::ZsyncParser()
{
  filesize = off_t(-1);
  blksize = 0;
  sql = rsl = csl = 0;
}

static int
hexstr2bytes(unsigned char *buf, const char *str, int buflen)
{
  int i = 0;
  for (i = 0; i < buflen; i++)
    {
#define c2h(c) (((c)>='0' && (c)<='9') ? ((c)-'0')              \
                : ((c)>='a' && (c)<='f') ? ((c)-('a'-10))       \
                : ((c)>='A' && (c)<='F') ? ((c)-('A'-10))       \
                : -1)
      int v = c2h(*str);
      str++;
      if (v < 0)
        return 0;
      buf[i] = v;
      v = c2h(*str);
      str++;
      if (v < 0)
        return 0;
      buf[i] = (buf[i] << 4) | v;
#undef c2h
    }
  return buflen;
}

void
ZsyncParser::parse( const Pathname &filename )
{
  char buf[4096];

  std::ifstream is(filename.c_str());
  if (!is)
    ZYPP_THROW(Exception("ZsyncParser: no such file"));
  is.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
  off_t filesize = off_t(-1);
  while (is.good())
    {
      is.getline(buf, sizeof(buf));
      if (!*buf)
        break;
      if (!strncmp(buf, "Length: ", 8))
        filesize = (off_t)strtoull(buf + 8, 0, 10);
      else if (!strncmp(buf, "Hash-Lengths: ", 14))
        (void)sscanf(buf + 14, "%d,%d,%d", &sql, &rsl, &csl);
      else if (!strncmp(buf, "Blocksize: ", 11))
        blksize = atoi(buf + 11);
      else if (!strncmp(buf, "URL: http://", 12) || !strncmp(buf, "URL: https://", 13) || !strncmp(buf, "URL: ftp://", 11) || !strncmp(buf, "URL: tftp://", 12) )
        urls.push_back(buf + 5);
      else if (!strncmp(buf, "SHA-1: ", 7))
        {
          unsigned char sha1[20];
          if (hexstr2bytes(sha1, buf + 7, 20) == 20)
            bl.setFileChecksum("SHA1", 20, sha1);
        }
    }
  if (filesize == off_t(-1))
    ZYPP_THROW(Exception("Parse Error"));
  if (blksize <= 0 || (blksize & (blksize - 1)) != 0)
    ZYPP_THROW(Exception("Parse Error: illegal block size"));
  bl.setFilesize(filesize);

  if (filesize)
    {
      if (csl < 3 || csl > 16 || rsl < 1 || rsl > 4 || sql < 1 || sql > 2)
        ZYPP_THROW(Exception("Parse Error: illegal hash lengths"));

      bl.setRsumSequence( sql );

      size_t nblks = (filesize + blksize - 1) / blksize;
      size_t i = 0;
      off_t off = 0;
      size_t size = blksize;
      for (i = 0; i < nblks; i++)
        {
          if (i == nblks - 1)
            {
              size = filesize % blksize;
              if (!size)
                size = blksize;
            }
          size_t blkno = bl.addBlock(off, size);
          unsigned char rp[16];
          rp[0] = rp[1] = rp[2] = rp[3] = 0;
          try {
            is.read((char *)rp + 4 - rsl, rsl);
          } catch ( const std::exception &e ) {
            if ( !is.good() ) {
              if (is.bad())
                throw zypp::Exception( "I/O error while reading" );
              else if (is.eof())
                throw zypp::Exception( "End of file reached unexpectedly" );
              else if (is.fail())
                throw zypp::Exception( "Non-integer data encountered" );
              else
                throw zypp::Exception( "Unknown IO err" );
            }
          }

          bl.setRsum(blkno, rsl, rp[0] << 24 | rp[1] << 16 | rp[2] << 8 | rp[3], blksize);
          try {
            is.read((char *)rp, csl);
          } catch ( const std::exception &e ) {
            if ( !is.good() ) {
              if (is.bad())
                throw zypp::Exception( "I/O error while reading" );
              else if (is.eof())
                throw zypp::Exception( "End of file reached unexpectedly" );
              else if (is.fail())
                throw zypp::Exception( "Non-integer data encountered" );
              else
                throw zypp::Exception( "Unknown IO err" );
            }
          }
          if ( !is.good() ) {
            if (is.bad())
              throw zypp::Exception( "I/O error while reading" );
            else if (is.eof())
              throw zypp::Exception( "End of file reached unexpectedly" );
            else if (is.fail())
              throw zypp::Exception( "Non-integer data encountered" );
            else
              throw zypp::Exception( "Unknown IO err" );
          }
          bl.setChecksum(blkno, "MD4", csl, rp, blksize);
          off += size;
        }
    }
  is.close();
  MIL << "Parsed " << urls.size() << " mirrors from " << filename << std::endl;
  if ( env::ZYPP_METALINK_DEBUG() ) {
    for ( const auto &url : urls )
      DBG << "- " <<  url << std::endl;
  }
}

std::vector<Url>
ZsyncParser::getUrls()
{
  std::vector<Url> ret;
  size_t i = 0;
  for (i = 0; i < urls.size(); i++)
    ret.push_back(Url(urls[i]));
  return ret;
}

MediaBlockList
ZsyncParser::getBlockList()
{
  return bl;
}

  } // namespace media
} // namespace zypp
