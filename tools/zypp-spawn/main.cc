#include "server.h"

#include <zypp-core/zyppng/io/private/zyppspawnengine_p.h>
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Regex.h>

#include <string>
#include <iostream>
#include <sys/prctl.h> // prctl(), PR_SET_PDEATHSIG

int main( int argc, char *argv[] )
{
  if ( argc < 2 )
    return 1;

  std::cerr << "Hello here zypp-spawn" << std::endl;

  zypp::str::regex reg("^[0-9]+$");
  zypp::str::smatch what;
  if ( !zypp::str::regex_match( argv[1], what, reg ) ) {
    std::cerr << "Invalid pid format: " << argv[1] << std::endl;
    exit(1);
  }

  // we got the zypp pid as cli param, we check later if that is actually the right one
  pid_t ppid = zypp::str::strtonum<pid_t>( argv[1] );

  // make sure this process exits with the zypp instance that created it
  int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
  if (r == -1) {
    perror("Failed to set PR_SET_PDEATHSIG");
  }

  // make sure the parent was not destroyed before we installed the death signal handler
  if ( ::getppid() != ppid ) {
    std::cerr << "PPID passed by argument does not match the current pid, assuming the parent died, exiting." << std::endl;
    exit(1);
  }

  // make sure a SIGINT to the process group does not affect us
  if ( ::signal( SIGINT, SIG_IGN ) == SIG_ERR )
    std::cerr << "Failed to set SIGINT handler." << std::endl;

  const std::string socketPath ( zypp::str::Format( zyppng::ZyppSpawnEngine::sockNameTemplate.data() ) % ppid );
  Server serv;
  return serv.run( socketPath );
}
