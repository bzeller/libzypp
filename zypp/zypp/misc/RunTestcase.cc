/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file  zypp/misc/RunTestcase.cc
 *
*/
#include "RunTestcase.h"

#include <zypp/base/Algorithm.h>
#include <zypp/ZYpp.h>
#include <zypp/ResPool.h>
#include <zypp/ResFilters.h>
#include <zypp/Capability.h>
#include <zypp/sat/Pool.h>
#include <zypp-core/base/LogControl.h>

#define ZYPP_USE_RESOLVER_INTERNALS
#include <zypp/solver/detail/Resolver.h>

#include "zypp/solver/detail/ItemCapKind.h"
#include "zypp/solver/detail/SolverQueueItem.h"
#include "zypp/solver/detail/SolverQueueItemDelete.h"
#include "zypp/solver/detail/SolverQueueItemInstall.h"
#include "zypp/solver/detail/SolverQueueItemInstallOneOf.h"
#include "zypp/solver/detail/SolverQueueItemLock.h"
#include "zypp/solver/detail/SolverQueueItemUpdate.h"



namespace zypp::misc::testcase {

  namespace {
    static constexpr std::string_view RESULT_MARKER(">!> ");

    Resolvable::Kind string2kind (const std::string & str)
    {
      const auto k = ResKind::fromBuiltin (str);
      if ( k == ResKind::nokind ) return ResKind::package;
      return k;
    }


    struct FindPackage
    {
      PoolItem poolItem;
      Resolvable::Kind kind;
      bool edition_set;
      Edition edition;
      bool arch_set;
      Arch arch;

      FindPackage (Resolvable::Kind k, const std::string & v, const std::string & r, const std::string & a)
        : kind (k)
        , edition_set( !v.empty() )
        , edition( v, r )
        , arch_set( !a.empty() )
        , arch( a )
      {
      }

      void _remember( PoolItem p )
      {
        poolItem = p;
      }

      bool operator()( PoolItem p)
      {
        if (arch_set && arch != p->arch()) {				// if arch requested, force this arch
          return true;
        }
        if (!p->arch().compatibleWith( ZConfig::instance().systemArchitecture() )) {
          return true;
        }

        if (edition_set) {
          if (p->edition().match( edition ) != 0)
            return true;
        }

        if (!poolItem							// none yet
             || (poolItem->arch().compare( p->arch() ) < 0)		// new has better arch
             || (poolItem->edition().compare( p->edition() ) < 0))	// new has better edition
        {
          _remember( p );
        }
        return true;
      }
    };
  }



  // ─── Impl ────────────────────────────────────────────────────────────────────

  struct RunTestcase::Impl
  {
    explicit Impl( const LoadTestcase & testcase )
      : _testcase( testcase )
    {}

    void report_info   ( const std::string & text ) { _report->message( TestcaseRunReport::MsgType::Info,    text ); }
    void report_warning( const std::string & text ) { _report->message( TestcaseRunReport::MsgType::Warning, text ); }
    void report_error  ( const std::string & text ) { _report->message( TestcaseRunReport::MsgType::Error,   text ); }
    void report_note   ( const std::string & text ) { _report->message( TestcaseRunReport::MsgType::Note,    text ); }

    PoolItem get_poolItem ( const std::string & source_alias, const std::string & package_name, const std::string & kind_name = "", const std::string & ver = "", const std::string & rel = "", const std::string & arch = "")
    {
      auto pool = ResPool::instance();
      PoolItem poolItem;
      Resolvable::Kind kind = string2kind (kind_name);

      try {
        FindPackage info (kind, ver, rel, arch);

        invokeOnEach( pool.byIdentBegin( kind,package_name ),
          pool.byIdentEnd( kind,package_name ),
          resfilter::ByRepository(source_alias),
          std::ref(info) );

        poolItem = info.poolItem;
        if (!poolItem) {
          // try to find the resolvable over all channel. This is useful for e.g. languages
          invokeOnEach( pool.byIdentBegin( kind,package_name ),
            pool.byIdentEnd( kind,package_name ),
            std::ref(info) );
          poolItem = info.poolItem;
        }
      }
      catch (Exception & excpt_r) {
        ZYPP_CAUGHT (excpt_r);
        report_error( "Can't find kind[" + kind_name + "]:'" + package_name + "': source '" + source_alias + "' not defined" );
        if (kind_name.empty())
          report_error( "Please specify kind=\"...\" in the <install.../> request." );
        return poolItem;
      }

      if (!poolItem) {
        report_error( "Can't find kind: " + kind.asString() + ":'" + package_name + "' in source '" + source_alias + "': no such name/kind" );
      }

      return poolItem;
    }

    const LoadTestcase & _testcase;
    zypp::solver::detail::SolverQueueItemList _solverQueue;
    callback::SendReport<TestcaseRunReport> _report;
  };

  // ─── RunTestcase ─────────────────────────────────────────────────────────────

  RunTestcase::RunTestcase( const LoadTestcase & testcase )
    : _pimpl( new Impl( testcase ) )
  {}

  RunTestcase::~RunTestcase()
  {}

  TrialResult RunTestcase::executeTrial( const TestcaseTrial & trial,
                                         zypp::ZYpp::Ptr zypp ) const
  {
    TrialResult result;

    // TODO: implement — migrate logic from deptestomatic::execute_trial verbatim,
    // then refactor to use library facilities where appropriate in a second pass.
    //
    // TODO: replace remaining outStream(_pimpl->_out/_err) calls with
    // _pimpl->report_info() / report_warning() / report_error() / report_note()
    // Access testcase via: _pimpl->_testcase
    // Access report via:   _pimpl->_report

    ResPool pool = ResPool::instance();
    zypp::solver::detail::Resolver resolver(pool);

    // ── Reset transact state so sequential trials start clean ─────────────────
    // if ( _firstTrial )
    resetPool ();


    // ── Apply solver flags from the testcase setup ────────────────────────────
    const TestcaseSetup & setup = _pimpl->_testcase.setupInfo();
    resolver.setFocus                   ( setup.resolverFocus()            );
    resolver.setIgnoreAlreadyRecommended( setup.ignorealreadyrecommended() );
    resolver.setOnlyRequires            ( setup.onlyRequires()             );
    resolver.setForceResolve            ( setup.forceResolve()             );
    resolver.setCleandepsOnRemove       ( setup.cleandepsOnRemove()        );
    resolver.setAllowDowngrade          ( setup.allowDowngrade()           );
    resolver.setAllowNameChange         ( setup.allowNameChange()          );
    resolver.setAllowArchChange         ( setup.allowArchChange()          );
    resolver.setAllowVendorChange       ( setup.allowVendorChange()        );
    resolver.dupSetAllowDowngrade       ( setup.dupAllowDowngrade()        );
    resolver.dupSetAllowNameChange      ( setup.dupAllowNameChange()       );
    resolver.dupSetAllowArchChange      ( setup.dupAllowArchChange()       );
    resolver.dupSetAllowVendorChange    ( setup.dupAllowVendorChange()     );

    for ( const auto &node: trial.nodes() ) {
      if ( node.name() == "note" ) {
        _pimpl->report_note( zypp::str::Str() << "NOTE: " << node.value() );
      } else if ( node.name() == "vverify" ) {	// see libzypp@ca9bcf16, testcase writer mixed "update" and "verify"
        verify = true;
      } else if ( node.name() == "current" ) {
        // unsupported
        //std::string source_alias = node.getProp("channel");
      } else if ( node.name() == "subscribe" ) {
        // unsupported
        //std::string source_alias = node.getProp("channel");
      } else if ( str::hasPrefix( node.name(), "install" ) && ( node.name().size() == 7 || node.name()[7] == ' ' ) ) {
        if ( node.name().size() > 7 ) {
          // job: install patch:openSUSE-SLE-15.4-2022-2831
          // as convenience for editing testcases
          std::string spec { str::trim( node.name().substr(8) ) };
          Capability cap { Capability::guessPackageSpec( spec ) };
          outStream(_pimpl->_err) << "Guessed " << cap << std::endl
               << "   from " << spec << std::endl;
          if ( cap )
            resolver.addExtraRequire( cap );
          else
            outStream(_pimpl->_err) << "Unknown job item " << node.name() << std::endl;
          continue;
        }
        // else:
       std::string source_alias = node.getProp ("channel");
       std::string name = node.getProp ("name");
       outStream(_pimpl->_out) <<  << std::endl;
        if (name.empty())
          name = node.getProp ("package");
       std::string kind_name = node.getProp ("kind");
       std::string soft = node.getProp ("soft");
       std::string version = node.getProp ("version");
       std::string release = node.getProp ("release");
       std::string architecture = node.getProp ("arch");

        PoolItem poolItem;

        poolItem = _pimpl->get_poolItem( source_alias, name, kind_name, version, release, architecture );
        if (poolItem) {
           outStream( _pimpl->_out ) << RESULT_MARKER << "Installing " << poolItem
                 << ((poolItem->kind() != ResKind::package) ? (poolItem->kind().asString() + ":") : "")
                 << name
                 << (version.empty()?"":(std::string("-")+poolItem->edition().version()))
                 << (release.empty()?"":(std::string("-")+poolItem->edition().release()))
                 << (architecture.empty()?"":(std::string(".")+poolItem->arch().asString()))
                 << " from channel " << source_alias << std::endl;
          poolItem.status().setToBeInstalled(ResStatus::USER);
          if (!soft.empty())
            poolItem.status().setSoftInstall(true);
        }
        else {
          outStream(_pimpl->_err) << "Unknown item " << source_alias << "::" << name <<std::endl;
          exit( 1 );
        }
      } else if ( node.name() == "uninstall") {

       std::string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");
       std::string kind_name = node.getProp ("kind");
       std::string soft = node.getProp ("soft");
       std::string version = node.getProp ("ver");
       std::string release = node.getProp ("rel");
       std::string architecture = node.getProp ("arch");

        PoolItem poolItem;

        poolItem = _pimpl->get_poolItem ("@System", name, kind_name, version, release, architecture );
        if (poolItem) {
           outStream( _pimpl->_out ) << RESULT_MARKER << "Uninstalling " << name
                 << (version.empty()?"":std::string("-")+poolItem->edition().version()))zypp::solver::detail::SolverQueueItemList _pimpl->_solverQueue;
                 << (release.empty()?"":std::string("-")+poolItem->edition().release()))
                 << (architecture.empty()?"":std::string(".")+poolItem->arch().asString()))
                 << std::endl;
          poolItem.status().setToBeUninstalled(ResStatus::USER);
          if (!soft.empty())
            poolItem.status().setSoftUninstall(true);
        } else {
          outStream(_pimpl->_err) << "Unknown system item " << name <<std::endl;
          exit( 1 );
        }
      } else if ( node.name() == "distupgrade" ) {

         outStream( _pimpl->_out ) << RESULT_MARKER << "Doing distribution upgrade ..." <<std::endl;
        resolver.doUpgrade();
        printKeept = true; // in print solution
        print_pool( resolver, MARKER );
      } else if ( node.name() == "update"|| node.name() == "verify" ) {	// see libzypp@ca9bcf16, testcase writer mixed "update" and "verify"

         outStream( _pimpl->_out ) << RESULT_MARKER << "Doing update ..." <<std::endl;
        resolver.setUpdateMode( true );  // Add an update all packages job rather than doing a 2nd solverrun
        resolver.resolvePool();
        // resolver.doUpdate();          // A 2nd solverrun would be wrong here. It resets auto-results from resolvePool
        print_solution (pool, instorder, printKeept );
        doUpdate = true;
      } else if ( node.name() == "instorder" || node.name() == "mediaorder" /*legacy*/ ) {

         outStream( _pimpl->_out ) << RESULT_MARKER << "Calculating installation order ..." <<std::endl;
        instorder = true;
      } else if ( node.name() == "whatprovides" ) {

       std::string kind_name = node.getProp ("kind");
       std::string prov_name = node.getProp ("provides");

        PoolItemSet poolItems;

        outStream(_pimpl->_out) << "poolItems providing '" << prov_name << "'" <<std::endl;

        poolItems = get_providing_poolItems (prov_name, kind_name);

        if (poolItems.empty()) {
          outStream(_pimpl->_err) << "None found" <<std::endl;
        } else {
          for (PoolItemSet::const_iterator iter = poolItems.begin(); iter != poolItems.end(); ++iter) {
            outStream(_pimpl->_out) << (*iter) <<std::endl;
          }
        }
      } else if ( node.name() == "addConflict" ) {
        std::vector<std::string> names;
        str::split( node.getProp ("name"), back_inserter(names), "," );
        const auto &kind = string2kind (node.getProp ("kind"));
        for (unsigned i=0; i < names.size(); ++i) {
          resolver.addExtraConflict(Capability (names[i], kind));
        }
      } else if ( node.name() == "addRequire" ) {
        std::vector<std::string> names;
        str::split( node.getProp ("name"), back_inserter(names), "," );
        const auto &kind = string2kind (node.getProp ("kind"));
        for (unsigned i=0; i < names.size(); ++i) {
          resolver.addExtraRequire(Capability (names[i], kind ));
        }
      } else if ( node.name() == "upgradeRepo" ) {
        std::vector<std::string> names;
        str::split( node.getProp ("name"), back_inserter(names), "," );
        if ( names.empty() ) {
          ERR << "upgradeRepo  'name' empty!" <<std::endl;
          outStream(_pimpl->_err) << "upgradeRepo 'name' empty!" <<std::endl;
          exit( 1 );
        }
        for (unsigned i=0; i < names.size(); ++i) {
          Repository r = satpool.reposFind( names[i] );
          if ( ! r )
          {
            ERR << "upgradeRepo '" << names[i] << "' not found. (been empty?)" <<std::endl;
            outStream(_pimpl->_err) << "upgradeRepo '" << names[i] << "' not found. (been empty?)" <<std::endl;
            exit( 1 );
          }
          else
            resolver.addUpgradeRepo( r );
        }
      } else if ( node.name() == "reportproblems" ) {
        bool success;
        if (!_pimpl->_solverQueue.empty())
          success = resolver.resolveQueue(_pimpl->_solverQueue);
        else
          success = resolver.resolvePool();
        if (success
             && node.getProp ("ignoreValidSolution").empty()) {
           outStream( _pimpl->_out ) << RESULT_MARKER << "No problems so far" <<std::endl;
        }
        else {
          print_problems( resolver );
        }
      } else if ( node.name() == "takesolution" ) {
       std::string problemNrStr = node.getProp ("problem");
       std::string solutionNrStr = node.getProp ("solution");
        assert (!problemNrStr.empty());
        assert (!solutionNrStr.empty());
        int problemNr = atoi (problemNrStr.c_str());
        int solutionNr = atoi (solutionNrStr.c_str());
         outStream( _pimpl->_out ) << RESULT_MARKER << "Want solution: " << solutionNr <<std::endl;
         outStream( _pimpl->_out ) << RESULT_MARKER << "For problem:   " << problemNr <<std::endl;
        ResolverProblemList problems = resolver.problems ();
         outStream( _pimpl->_out ) << RESULT_MARKER << "*T*(" << resolver.problems().size() << ")" <<std::endl;

        int problemCounter = -1;
        int solutionCounter = -1;
        // find problem
        for (ResolverProblemList::iterator probIter = problems.begin(); probIter != problems.end(); ++probIter) {
          problemCounter++;
           outStream( _pimpl->_out ) << RESULT_MARKER << "*P*(" << problemCounter << "|" << solutionCounter << ")" <<std::endl;
          if (problemCounter == problemNr) {
            ResolverProblem problem = **probIter;
            ProblemSolutionList solutionList = problem.solutions();
            //find solution
            for (ProblemSolutionList::iterator solIter = solutionList.begin();
              solIter != solutionList.end(); ++solIter) {
              solutionCounter++;
               outStream( _pimpl->_out ) << RESULT_MARKER << "*S*(" << problemCounter << "|" << solutionCounter << ")" <<std::endl;
              if (solutionCounter == solutionNr) {
                ProblemSolution_Ptr solution = *solIter;
                 outStream( _pimpl->_out ) << RESULT_MARKER << "Taking solution: " <<outStream(_pimpl->_err) << *solution <<std::endl;
                 outStream( _pimpl->_out ) << RESULT_MARKER << "For problem: " <<outStream(_pimpl->_err) << problem <<std::endl;
                ProblemSolutionList doList;
                doList.push_back (solution);
                resolver.applySolutions (doList);
                break;
              }
            }
            break;
          }
        }

        if ( problemCounter != problemNr || solutionCounter != solutionNr ) {
           outStream( _pimpl->_out ) << RESULT_MARKER << "Did not find problem=" << problemCounter << ", solution="  << solutionCounter <<std::endl;
        } else {
          // resolve and check it again
          bool success;
          if (!_pimpl->_solverQueue.empty())
            success = resolver.resolveQueue(_pimpl->_solverQueue);
          else
            success = resolver.resolvePool();

          if (success) {
             outStream( _pimpl->_out ) << RESULT_MARKER << "No problems so far" <<std::endl;
          }
          else {
            ResolverProblemList problems = resolver.problems ();
             outStream( _pimpl->_out ) << RESULT_MARKER << problems.size() << " problems found:" <<std::endl;
            for (ResolverProblemList::iterator iter = problems.begin(); iter != problems.end(); ++iter) {
              outStream(_pimpl->_out) << **iter <<std::endl;
            }
          }
        }
      } else if ( node.name() == "showpool" ) {
        resolver.resolvePool();
       std::string prefix = node.getProp ("prefix");
       std::string all = node.getProp ("all");
       std::string get_licence = node.getProp ("getlicence");
       std::string verbose = node.getProp ("verbose");
        print_pool( resolver, prefix, !all.empty(), get_licence, !verbose.empty() );
        printKeept = true; // in print solution
      } else if ( node.name() == "showstatus" ) {
        resolver.resolvePool();
       std::string prefix = node.getProp ("prefix");
       std::string all = node.getProp ("all");
       std::string get_licence = node.getProp ("getlicence");
       std::string verbose = node.getProp ("verbose");
        print_pool( resolver, "", true, "false", true );

      } else if (node.name() == "showselectable" ){
      ui::Selectable::Ptr item;
       std::string kind_name = node.getProp ("kind");
        if ( kind_name.empty() )
          kind_name = "package";
       std::string name = node.getProp ("name");
        item = ui::Selectable::get( ResKind(kind_name), name );
        if ( item )
          dumpOn(outStream(_pimpl->_out), *item);
        else
          outStream(_pimpl->_out) << "Selectable '" << name << "' not valid" <<std::endl;
      } else if ( node.name() == "graphic" ) {
         outStream( _pimpl->_out ) << RESULT_MARKER << "<graphic> is no longer supported by deptestomatic" <<std::endl;
      } else if ( node.name() == ("YOU") || node.name() == ("PkgUI") ) {
         outStream( _pimpl->_out ) << RESULT_MARKER << "<YOU> or <PkgUI> are no longer supported by deptestomatic" <<std::endl;
      } else if ( node.name() == "lock" ) {
       std::string source_alias = node.getProp ("channel");
       std::string package_name = node.getProp ("name");
        if (package_name.empty())
          package_name = node.getProp ("package");
       std::string kind_name = node.getProp ("kind");
       std::string version = node.getProp ("version");
        if ( version.empty() )
          version = node.getProp ("ver");
       std::string release = node.getProp ("release");
        if ( release.empty() )
          release = node.getProp ("rel");
       std::string architecture = node.getProp ("arch");

        if ( version.empty() )
        {
          if ( kind_name.empty() )
            kind_name = "package";
          Selectable::Ptr item = Selectable::get( ResKind(kind_name), package_name );
          if ( item )
          {
            item->setStatus( item->hasInstalledObj() ? ui::S_Protected : ui::S_Taboo );
            item->setStatus( ui::S_Taboo );
          }
          else
          {
            outStream(_pimpl->_err) << "Unknown Selectable " << kind_name << ":" << package_name <<std::endl;
          }
        }
        else
        {
          PoolItem poolItem;
          poolItem = _pimpl->get_poolItem (source_alias, package_name, kind_name, version, release, architecture );
          if (poolItem) {
             outStream( _pimpl->_out ) << RESULT_MARKER << "Locking " << package_name << " from channel " << source_alias << poolItem <<std::endl;
            poolItem.status().setLock (true, ResStatus::USER);
          } else {
            outStream(_pimpl->_err) << "Unknown package " << source_alias << "::" << package_name <<std::endl;
          }
        }
      } else if ( node.name() == "validate" ) {
       std::string source_alias = node.getProp ("channel");
       std::string package_name = node.getProp ("name");
        if (package_name.empty())
          package_name = node.getProp ("package");
       std::string kind_name = node.getProp ("kind");
       std::string version = node.getProp ("ver");
       std::string release = node.getProp ("rel");
       std::string architecture = node.getProp ("arch");

        // Solving is needed
        if (!_pimpl->_solverQueue.empty())
          resolver.resolveQueue(_pimpl->_solverQueue);
        else
          resolver.resolvePool();

        if (!package_name.empty()) {
          PoolItem poolItem;
          poolItem = _pimpl->get_poolItem (source_alias, package_name, kind_name, version, release, architecture );
          if (poolItem) {
            if (poolItem.isSatisfied())
               outStream( _pimpl->_out ) << RESULT_MARKER <<  package_name << " from channel " << source_alias << " IS SATISFIED" <<std::endl;
            else
               outStream( _pimpl->_out ) << RESULT_MARKER <<  package_name << " from channel " << source_alias << " IS NOT SATISFIED" <<std::endl;
          } else {
            outStream(_pimpl->_err) << "Unknown package " << source_alias << "::" << package_name <<std::endl;
          }
        } else {
          // Checking all resolvables
          isSatisfied (kind_name);
        }
      } else if ( node.name() == "availablelocales" ) {
         outStream( _pimpl->_out ) << RESULT_MARKER << "Available locales: ";
        LocaleSet locales = pool.getAvailableLocales();
        for (LocaleSet::const_iterator it = locales.begin(); it != locales.end(); ++it) {
          if (it != locales.begin()) std::outStream(_pimpl->_out) << ", ";
          std::outStream(_pimpl->_out) << it->code();
        }
        std::outStream(_pimpl->_out) <<std::endl;

      } else if ( node.name() == "keep" ) {
       std::string kind_name = node.getProp ("kind");
       std::string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");

       std::string source_alias = node.getProp ("channel");
        if (source_alias.empty())
          source_alias = "@System";

        if (name.empty())
        {
          outStream(_pimpl->_err) << "transact need 'name' parameter" <<std::endl;
          return;
        }

        PoolItem poolItem;

        poolItem = _pimpl->get_poolItem( source_alias, name, kind_name, node.getProp ("version"), node.getProp ("release") );

        if (poolItem) {
          // first: set anything
          if (source_alias == "@System") {
            poolItem.status().setToBeUninstalled( ResStatus::USER );
          }
          else {
            poolItem.status().setToBeInstalled( ResStatus::USER );
          }
          // second: keep old state
          poolItem.status().setTransact( false, ResStatus::USER );
        }
        else {
          outStream(_pimpl->_err) << "Unknown item " << source_alias << "::" << name <<std::endl;
        }
      } else if ( node.name() == "addQueueInstall" ) {
       std::string name = node.getProp ("name");
       std::string soft = node.getProp ("soft");

        if (name.empty()) {
          outStream(_pimpl->_err) << "addQueueInstall need 'name' parameter" <<std::endl;
          return;
        }
        zypp::solver::detail::SolverQueueItemInstall_Ptr install =
          new zypp::solver::detail::SolverQueueItemInstall(pool, name, (soft.empty() ? false : true));
        _pimpl->_solverQueue.push_back (install);
      } else if ( node.name() == "addQueueDelete" ) {
       std::string name = node.getProp ("name");
       std::string soft = node.getProp ("soft");

        if (name.empty()) {
          outStream(_pimpl->_err) << "addQueueDelete need 'name' parameter" <<std::endl;
          return;
        }
        zypp::solver::detail::SolverQueueItemDelete_Ptr del =
          new zypp::solver::detail::SolverQueueItemDelete(pool, name, (soft.empty() ? false : true));
        _pimpl->_solverQueue.push_back (del);
      } else if ( node.name() == "addQueueLock" ) {
       std::string soft = node.getProp ("soft");
       std::string kind_name = node.getProp ("kind");
       std::string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");

       std::string source_alias = node.getProp ("channel");
        if (source_alias.empty())
          source_alias = "@System";

        if (name.empty())
        {
          outStream(_pimpl->_err) << "transact need 'name' parameter" <<std::endl;
          return;
        }

        PoolItem poolItem = _pimpl->get_poolItem( source_alias, name, kind_name );
        if (poolItem) {
          zypp::solver::detail::SolverQueueItemLock_Ptr lock =
            new zypp::solver::detail::SolverQueueItemLock(pool, poolItem, (soft.empty() ? false : true));
          _pimpl->_solverQueue.push_back (lock);
        }
        else {
          outStream(_pimpl->_err) << "Unknown item " << source_alias << "::" << name <<std::endl;
        }
      } else if ( node.name() == "addQueueUpdate" ) {
       std::string kind_name = node.getProp ("kind");
       std::string name = node.getProp ("name");
        if (name.empty())
          name = node.getProp ("package");

       std::string source_alias = node.getProp ("channel");
        if (source_alias.empty())
          source_alias = "@System";

        if (name.empty())
        {
          outStream(_pimpl->_err) << "transact need 'name' parameter" < <std::endl;
          return;
        }

        PoolItem poolItem = _pimpl->get_poolItem( source_alias, name, kind_name );
        if (poolItem) {
          zypp::solver::detail::SolverQueueItemUpdate_Ptr lock =
            new zypp::solver::detail::SolverQueueItemUpdate(pool, poolItem);
          _pimpl->_solverQueue.push_back (lock);
        }
        else {
          outStream(_pimpl->_err) << "Unknown item " << source_alias << "::" << name <<std::endl;
        }
      } else if ( node.name() == "addQueueInstallOneOf" ) {
        zypp::solver::detail::PoolItemList poolItemList;
        for ( const auto &child : node.children() ) {
          if ( child->name() == "item" ) {
           std::string kind_name = child->getProp ("kind");
           std::string name = child->getProp ("name");
            if (name.empty())
              name = child->getProp ("package");

           std::string source_alias = child->getProp ("channel");
            if (source_alias.empty())
              source_alias = "@System";

            if (name.empty()) {
              outStream(_pimpl->_err) << "transact need 'name' parameter" <<std::endl;
            } else {
              PoolItem poolItem = _pimpl->get_poolItem( source_alias, name, kind_name );
              if (poolItem) {
                poolItemList.push_back(poolItem);
              }
              else {
                outStream(_pimpl->_err) << "Unknown item " << source_alias << "::" << name <<std::endl;
              }
            }
          } else {
            outStream(_pimpl->_err) << "addQueueInstallOneOf: cannot find flag 'item'" <<std::endl;
          }
        }
        if (poolItemList.empty()) {
          outStream(_pimpl->_err) << "addQueueInstallOneOf has an empty list" <<std::endl;
          return;
        } else {
          zypp::solver::detail::SolverQueueItemInstallOneOf_Ptr install =
            new zypp::solver::detail::SolverQueueItemInstallOneOf(pool, poolItemList);
          _pimpl->_solverQueue.push_back (install);
        }
      } else if ( node.name() == "createTestcase" ) {
       std::string path = node.getProp ("path");
        if (path.empty())
          path = "./solverTestcase";
        Testcase testcase (path);
        testcase.createTestcase (*resolver);
      } else {
        ERR << "Unknown tag '" << node.name() << "' in trial" <<std::endl;
        outStream(_pimpl->_err) << "Unknown tag '" << node.name() << "' in trial" <<std::endl;
      }
    }
    return result;
  }

  void RunTestcase::resetPool() const
  {
    ResPool pool = ResPool::instance();
    for (ResPool::const_iterator it = pool.begin(); it != pool.end(); ++it) {
      if (it->status().transacts()) it->status().resetTransact( ResStatus::USER );
    }
  }

} // namespace zypp::misc::testcase
