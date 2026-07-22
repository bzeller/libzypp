/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file  zypp/misc/LoadTestcase.cc
 *
*/
#include "LoadTestcase.h"
#include "HelixHelpers.h"
#include "YamlTestcaseHelpers.h"
#include "TestcaseSetupImpl.h"
#include <zypp/PathInfo.h>
#include <zypp-core/base/LogControl.h>
#include <zypp/ZYpp.h>
#include <zypp/Resolver.h>
#include <zypp/ResPool.h>
#include <zypp/ResFilters.h>
#include <zypp/Capability.h>
#include <zypp/sat/Pool.h>

namespace zypp::misc::testcase {
  static const std::string helixControlFile = "solver-test.xml";
  static const std::string yamlControlFile  = "zypp-control.yaml";

  struct LoadTestcase::Impl {
    TestcaseSetup _setup;
    TestcaseTrials _trials;

    bool loadHelix (const Pathname &filename, std::string *err);

    bool loadYaml  ( const Pathname &path, std::string *err);
  };


  struct TestcaseTrial::Impl
  {
    std::vector<Node> nodes;
    Impl *clone() const { return new Impl(*this); }
  };

  struct TestcaseTrial::Node::Impl
  {
    std::string name;
    std::string value;
    std::map<std::string, std::string> properties;
    std::vector<std::shared_ptr<Node>> children;
    Impl *clone() const { return new Impl(*this); }
  };

  TestcaseTrial::TestcaseTrial() : _pimpl ( new Impl() )
  { }

  TestcaseTrial::~TestcaseTrial()
  { }

  const std::vector<TestcaseTrial::Node> &TestcaseTrial::nodes() const
  { return _pimpl->nodes; }

  std::vector<TestcaseTrial::Node> &TestcaseTrial::nodes()
  { return _pimpl->nodes; }

  TestcaseTrial::Node::Node() : _pimpl ( new Impl() )
  { }

  TestcaseTrial::Node::~Node()
  { }

  const std::string &TestcaseTrial::Node::name() const
  { return _pimpl->name; }

  std::string &TestcaseTrial::Node::name()
  { return _pimpl->name; }

  const std::string &TestcaseTrial::Node::value() const
  { return _pimpl->value; }

  std::string &TestcaseTrial::Node::value()
  { return _pimpl->value; }

  const std::string &TestcaseTrial::Node::getProp( const std::string &name, const std::string &def ) const
  {
    auto it { _pimpl->properties.find( name ) };
    return it == _pimpl->properties.end() ? def : it->second;
  }

  const std::map<std::string, std::string> &TestcaseTrial::Node::properties() const
  { return _pimpl->properties; }

  std::map<std::string, std::string> &TestcaseTrial::Node::properties()
  { return _pimpl->properties; }

  const std::vector<std::shared_ptr<TestcaseTrial::Node> > &TestcaseTrial::Node::children() const
  { return _pimpl->children; }

  std::vector<std::shared_ptr<TestcaseTrial::Node> > &TestcaseTrial::Node::children()
  { return _pimpl->children; }

  bool LoadTestcase::Impl::loadHelix(const zypp::filesystem::Pathname &filename, std::string *err)
  {
    xmlDocPtr xml_doc = xmlParseFile ( filename.c_str() );
    if (xml_doc == NULL) {
      if ( err ) *err = (str::Str() << "Can't parse test file '" << filename << "'");
      return false;
    }


    auto root = helix::detail::XmlNode (xmlDocGetRootElement (xml_doc));

    DBG << "Parsing file '" << filename << "'" << std::endl;

    if (!root.equals("test")) {
      if ( err ) *err = (str::Str() << "Node not 'test' in parse_xml_test():" << root.name() << "'");
      return false;
    }

    bool setupDone = false;
    auto node = root.children();
    while (node) {
      if (node->type() == XML_ELEMENT_NODE) {
        if (node->equals( "setup" )) {
          if ( setupDone ) {
            if ( err ) *err = "Multiple setup tags found, this is not supported";
            return false;
          }
          setupDone = true;
          if ( !helix::detail::parseSetup( *node, _setup, err ) )
            return false;

        } else if (node->equals( "trial" )) {
          if ( !setupDone ) {
            if ( err ) *err = "Any trials must be preceded by the setup!";
            return false;
          }
          TestcaseTrial trial;
          if ( !helix::detail::parseTrial( *node, trial, err ) )
            return false;
          _trials.push_back( trial );
        } else {
          ERR << "Unknown tag '" << node->name() << "' in test" << std::endl;
        }
      }
      node = ( node->next() );
    }
    xmlFreeDoc (xml_doc);
    return true;
  }

  bool LoadTestcase::Impl::loadYaml(const zypp::filesystem::Pathname &path, std::string *err)
  {
    DBG << "Parsing file '" << path << "'" << std::endl;

    const auto makeError = [&]( const std::string_view &errStr ){
      if ( err ) *err = errStr;
      return false;
    };

    YAML::Node control;
    try {
      control = YAML::LoadFile( path.asString() );

      if ( control.Type() != YAML::NodeType::Map )
        return makeError("Root node must be of type Map.");

      const auto &setup = control["setup"];
      if ( !setup )
        return makeError("The 'setup' section is required.");

      if ( !yamltest::detail::parseSetup( setup, _setup, err) )
        return false;

      const auto &trials = control["trials"];
      if ( !trials )
        return makeError("The 'trials' section is required.");
      if ( trials.Type() != YAML::NodeType::Sequence )
        return makeError("The 'trials' section must be of type Sequence.");

      for ( const auto &trial : trials ) {
        zypp::misc::testcase::TestcaseTrial t;
        if ( !trial["trial"] )
          return makeError("Every element in the trials sequence needs to have the 'trial' key.");

        if ( !yamltest::detail::parseTrial( trial["trial"], t, _setup.data().globalPath, err) )
          return false;
        _trials.push_back( t );
      }
    } catch ( YAML::Exception &e ) {
      if ( err ) *err = e.what();
      return false;
    } catch ( ... )  {
      if ( err ) *err = "Unknown error when parsing the control file";
      return false;
    }
    return true;
  }

  LoadTestcase::LoadTestcase() : _pimpl( new Impl() )
  { }

  LoadTestcase::~LoadTestcase()
  { }

  bool LoadTestcase::loadTestcaseAt(const zypp::filesystem::Pathname &path, std::string *err)
  {
    const auto t = testcaseTypeAt( path );
    if ( t == LoadTestcase::None ) {
      if ( err ) *err = "Unsopported or no testcase in directory";
      return false;
    }

    // reset everything
    _pimpl.reset( new Impl() );
    _pimpl->_setup.data().globalPath = path;

    switch (t) {
      case LoadTestcase::Helix:
        if ( !_pimpl->loadHelix( path / helixControlFile, err ) )
          return false;
        break;
      case LoadTestcase::Yaml:
        if ( !_pimpl->loadYaml( path / yamlControlFile, err ) )
          return false;
        break;
      default:
        return false;
    }

    // ── Extract lock/keep nodes from all trials into the setup ─────────────────
    // Done as a post-parse pass so neither parser needs to be modified.
    // Locks are pool constraints that belong to the environment, not the job.
    for ( const auto & trial : _pimpl->_trials )
      for ( const auto & node : trial.nodes() )
        if ( node.name() == "lock" || node.name() == "keep" )
          _pimpl->_setup.data().locks.push_back( { node.name(), node.properties() } );

    return true;
  }

  LoadTestcase::Type LoadTestcase::testcaseTypeAt(const zypp::filesystem::Pathname &path)
  {
    if ( filesystem::PathInfo( path / helixControlFile ).isFile() ) {
      return LoadTestcase::Helix;
    } else if ( filesystem::PathInfo( path / yamlControlFile ).isFile() ) {
      return LoadTestcase::Yaml;
    }
    return LoadTestcase::None;
  }

  const TestcaseSetup &LoadTestcase::setupInfo() const
  {
    return _pimpl->_setup;
  }

  void LoadTestcase::applyLockEntry(const TestcaseSetup::LockEntry &entry, ResPool &pool, std::ostream * out, std::ostream * err )
  {
    const bool isKeep = ( entry.first == "keep" );
    const auto & props = entry.second;

    auto getProp = [&]( const std::string & k ) -> std::string {
      auto it = props.find(k);
      return it != props.end() ? it->second : std::string();
    };

    if ( !isKeep ) {
      std::string source_alias = getProp ("channel");
      std::string package_name = getProp ("name");
      if (package_name.empty())
        package_name = getProp ("package");
      std::string kind_name = getProp ("kind");
      std::string version = getProp ("version");
      if ( version.empty() )
        version = getProp ("ver");
      std::string release = getProp ("release");
      if ( release.empty() )
        release = getProp ("rel");
      std::string architecture = getProp ("arch");

      if ( version.empty() )
      {
        if ( kind_name.empty() )
          kind_name = "package";
        ui::Selectable::Ptr item = ui::Selectable::get( ResKind(kind_name), package_name );
        if ( item )
        {
          item->setStatus( item->hasInstalledObj() ? ui::S_Protected : ui::S_Taboo );
          item->setStatus( ui::S_Taboo );
        }
        else
        {
          outStream (err) << "Unknown Selectable " << kind_name << ":" << package_name << std::endl;
        }
      }
      else
      {
        PoolItem poolItem;
        poolItem = get_poolItem (source_alias, package_name, kind_name, version, release, architecture );
        if (poolItem) {
          outStream (out) << RESULT_MARKER << "Locking " << package_name << " from channel " << source_alias << poolItem << std::endl;
          poolItem.status().setLock (true, ResStatus::USER);
        } else {
          outStream (err) << "Unknown package " << source_alias << "::" << package_name << std::endl;
        }
      }

    } else {






    }
  }

  const LoadTestcase::TestcaseTrials &LoadTestcase::trialInfo() const
  {
    return _pimpl->_trials;
  }

} // namespace zypp::misc::testcase
