/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file  zypp/misc/RunTestcase.h
 *
 * Execution engine for solver testcases.
 *
 * \ref LoadTestcase is responsible for parsing testcase data.
 * \ref RunTestcase is responsible for executing it against a live pool.
 */
#ifndef ZYPP_MISC_RUNTESTCASE_H
#define ZYPP_MISC_RUNTESTCASE_H

#include <zypp/ZYpp.h>
#include <zypp/PoolItem.h>
#include <zypp/ResolverProblem.h>
#include <zypp/Callback.h>
#include <zypp/misc/LoadTestcase.h>
#include <zypp-core/base/NonCopyable.h>

#include <memory>
#include <string>
#include <vector>

namespace zypp::misc::testcase {

  /** Report fired during testcase trial execution.
   *
   * Connect a \ref callback::ReceiveReport<TestcaseRunReport> to intercept
   * messages, trial lifecycle events, and results.  The default implementations
   * are all no-ops so unconnected runs are silent.
   *
   * \code
   * struct MyReceiver : callback::ReceiveReport<TestcaseRunReport>
   * {
   *     void message( MsgType type, const std::string & text ) override
   *     { std::cout << text << '\n'; }
   * };
   *
   * MyReceiver receiver;
   * receiver.connect();
   * runner.executeTrial( trial, zypp );
   * receiver.disconnect();
   * \endcode
   */
  struct ZYPP_API_DEPTESTOMATIC TestcaseRunReport : public callback::ReportBase
  {
    enum class MsgType {
      Note,     ///< <note> node content
      Info,     ///< progress / result (e.g. "Installing foo-1.0")
      Warning,  ///< recoverable issue (e.g. "Unknown item: bar")
      Error     ///< hard error
    };

    /** A message produced during trial execution. */
    virtual void message( MsgType /*type*/, const std::string & /*text*/ )
    {}

    /** Called before the first trial node is dispatched. */
    virtual void trialBegin( const TestcaseTrial & /*trial*/ )
    {}

    /** Called after resolvePool() — before returning the TrialResult. */
    virtual void trialEnd( bool /*success*/ )
    {}
  };

  /** Result of executing a single solver trial.
   *
   * Intentionally minimal — pool state (toInstall/toRemove) is queryable
   * directly via ResPool::instance() after the trial returns.
   * Messages are delivered via \ref TestcaseRunReport, not stored here.
   * Problems are stored here because the resolver is not accessible after
   * executeTrial returns.
   */
  struct ZYPP_API_DEPTESTOMATIC TrialResult
  {
    bool                   success  = false;
    ResolverProblemList    problems;
  };

  /** Execution engine for solver testcases.
   *
   * Not copyable — holds a reference to the source \ref LoadTestcase.
   *
   * Connect a \ref TestcaseRunReport receiver before calling executeTrial
   * to receive progress messages.  Without a connected receiver all output
   * is suppressed.
   *
   * Typical usage:
   * \code
   * LoadTestcase loader;
   * loader.loadTestcaseAt( path, &err );
   *
   * RepoManager mgr( RepoManagerOptions::makeTestSetup( path ) );
   * loader.setupInfo().applySetup( mgr, AS_UNIVERSE );
   * sat::Pool::instance().prepare();
   *
   * RunTestcase runner( loader );
   * for ( const auto & trial : loader.trialInfo() )
   * {
   *     TrialResult r = runner.executeTrial( trial, zypp );
   * }
   * \endcode
   */
  class ZYPP_API_DEPTESTOMATIC RunTestcase : private base::NonCopyable
  {
  public:
    struct Impl;

    explicit RunTestcase( const LoadTestcase & testcase );
    ~RunTestcase();

    /** Execute a single trial against an already-loaded pool.
     *
     * Resets all transact states at entry so trials can be executed
     * sequentially on the same pool without interference.
     *
     * Solver flags from the testcase setup are applied before resolving.
     *
     * Progress and messages are delivered via \ref TestcaseRunReport.
     *
     * \param trial   The trial to execute.
     * \param zypp    An already-initialised ZYpp instance with the pool loaded.
     */
    TrialResult executeTrial( const TestcaseTrial & trial,
                              zypp::ZYpp::Ptr zypp ) const;

    /** Reset all transact states in the pool.
     *  Called automatically at the start of each executeTrial, but also
     *  available for callers that need to reset between manual operations.
     */
    void resetPool() const;

  private:
    std::unique_ptr<Impl> _pimpl;
  };

} // namespace zypp::misc::testcase

#endif // ZYPP_MISC_RUNTESTCASE_H
