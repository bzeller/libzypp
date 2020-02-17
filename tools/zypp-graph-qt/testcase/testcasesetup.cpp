#include "testcasesetup.h"
#include "channel.h"
#include "forceinstall.h"
#include "source.h"
#include "localechange.h"

#include <QXmlStreamReader>
#include <QFile>
#include <QDebug>
#include <functional>
#include <zypp/base/Logger.h>

TestcaseSetup::TestcaseSetup(QObject *parent) : QObject(parent)
{
  loadTestcase( "/home/zbenjamin/workspace/solvertestcase/zypper.solverTestCase/solver-test.xml" );
}

bool TestcaseSetup::loadTestcase( const QString &fileName )
{

  QFile file( fileName );
  if (!file.open(QIODevice::ReadOnly))
    return false;

  QXmlStreamReader reader( &file );

  auto readAttrib = [ &reader, this ]( const auto &name, auto callback ){
    const auto attrib = reader.attributes();
    if ( attrib.hasAttribute(name) )
      std::invoke( callback, *this, attrib.value(name).toString() );
  };

  if (reader.readNextStartElement()) {
    if (reader.name() == QStringLiteral("test") ) {
      reader.readNextStartElement();
      if ( reader.name() == QStringLiteral("setup") ) {

        {
          const auto attrib = reader.attributes();
          if ( attrib.hasAttribute("arch") )
            setArch( attrib.value("arch").toString() );
        }

        while ( reader.readNextStartElement() ) {
          const QStringRef elemName = reader.name();
          if ( elemName == QStringLiteral("system") ) {
            //readAttrib( "file", &TestcaseSetup::m_systemRepo );
          } else if ( elemName == QStringLiteral("channel") ) {
            const auto attrib = reader.attributes();
            QString name = attrib.value("name").toString();
            QString file = attrib.value("file").toString();
            QString type = attrib.value("type").toString();

            bool ok = false;
            unsigned priority = attrib.value("priority").toUInt( &ok );
            if ( !ok )
              priority = 99;
            addChannel( new Channel( name, file, type ,priority, this ) );
          } else if ( elemName == QStringLiteral("autoinst") ) {
            readAttrib( "name", &TestcaseSetup::addAutoInstall );
          } else if ( elemName == QStringLiteral("focus") ) {
            readAttrib( "value", &TestcaseSetup::setResolverFocusFromString );
          } else if ( elemName == QStringLiteral("hardwareInfo") ) {
            readAttrib( "path", &TestcaseSetup::setHardwareInfo );
          } else if ( elemName == QStringLiteral("modalias") ) {
            readAttrib( "name", &TestcaseSetup::addModalias );
          } else if ( elemName == QStringLiteral("multiversion") ) {
            readAttrib( "name", &TestcaseSetup::addMultiversion );
          } else if ( elemName == QStringLiteral("source") ) {
            const auto attrib = reader.attributes();
            addSource( new Source(
              attrib.value(("url")).toString(),
              attrib.value("name").toString(),
              this
              ));
          } else if ( elemName == QStringLiteral("mediaid") ) {
            setShowMediaId( true );
          } else if ( elemName == QStringLiteral("force-install") ) {
            const auto attrib = reader.attributes();
            addForceInstall( new ForceInstall(
              attrib.value("channel").toString(),
              attrib.value("package").toString(),
              attrib.value("kind").toString(),
              this
              ));
          } else if ( elemName == QStringLiteral("arch") ) {
            MIL << "<arch...> deprecated, use <setup arch=\"...\"> instead" << std::endl;
            setArch( reader.attributes().value("name").toString() );
            if ( m_arch.isEmpty() ) {
              MIL << "Property 'name=' in <arch.../> missing or empty" << std::endl;
            }
          } else if ( elemName == QStringLiteral("locale") ) {
            const auto attrib = reader.attributes();
            if ( attrib.hasAttribute("name") && attrib.hasAttribute("fate") ) {
              const auto fate = attrib.value("fate");
              auto fateVal = LocaleChange::Current;
              if ( fate == QStringLiteral("added") )
                fateVal = LocaleChange::Added;
              else if ( fate == QStringLiteral("removed") )
                fateVal = LocaleChange::Removed;
              m_locales.push_back( new LocaleChange( attrib.value("name").toString() ,fateVal ,this ) );
            }
          } else if ( elemName == QStringLiteral("systemCheck") ) {
            setSystemCheck( reader.attributes().value("path").toString() );
          }
          else if ( elemName == QStringLiteral("setlicencebit") )             { setLicencebit(true); }
          else if ( elemName == QStringLiteral("ignorealreadyrecommended" ) ) { setIgnorealreadyrecommended(true); }
          else if ( elemName == QStringLiteral("ignorealready" ) )            { setIgnorealreadyrecommended(true); }
          else if ( elemName == QStringLiteral("onlyRequires" ) )             { setOnlyRequires(true); }
          else if ( elemName == QStringLiteral("ignorerecommended" ) )        { setOnlyRequires(true); }
          else if ( elemName == QStringLiteral("forceResolve" ) )             { setForceResolve(true); }
          else if ( elemName == QStringLiteral("cleandepsOnRemove" ) )        { setCleandepsOnRemove(true); }
          else if ( elemName == QStringLiteral("allowDowngrade" ) )           { setAllowDowngrade(true); }
          else if ( elemName == QStringLiteral("allowNameChange" ) )          { setAllowNameChange(true); }
          else if ( elemName == QStringLiteral("allowArchChange" ) )          { setAllowArchChange(true); }
          else if ( elemName == QStringLiteral("allowVendorChange" ) )        { setAllowVendorChange(true); }
          else if ( elemName == QStringLiteral("dupAllowDowngrade" ) )        { setDupAllowDowngrade(true); }
          else if ( elemName == QStringLiteral("dupAllowNameChange" ) )       { setDupAllowNameChange(true); }
          else if ( elemName == QStringLiteral("dupAllowArchChange" ) )       { setDupAllowArchChange(true); }
          else if ( elemName == QStringLiteral("dupAllowVendorChange" ) )     { setDupAllowVendorChange(true); }

          reader.skipCurrentElement();
        }
      } else if ( reader.name() == QStringLiteral("trial") ) {
        reader.skipCurrentElement();
      } else {
        reader.skipCurrentElement();
      }
    } else {
      reader.raiseError(QObject::tr("The file is not a zypp testcase file."));
    }
  }

  file.close();
  return !reader.hasError();
}

bool TestcaseSetup::ignorealreadyrecommended() const
{
  return m_ignorealreadyrecommended;
}

bool TestcaseSetup::onlyRequires() const
{
  return m_onlyRequires;
}

bool TestcaseSetup::forceResolve() const
{
  return m_forceResolve;
}

bool TestcaseSetup::cleandepsOnRemove() const
{
  return m_cleandepsOnRemove;
}

bool TestcaseSetup::allowDowngrade() const
{
  return m_allowDowngrade;
}

bool TestcaseSetup::allowNameChange() const
{
  return m_allowNameChange;
}

bool TestcaseSetup::allowArchChange() const
{
  return m_allowArchChange;
}

bool TestcaseSetup::allowVendorChange() const
{
  return m_allowVendorChange;
}

bool TestcaseSetup::dupAllowDowngrade() const
{
  return m_dupAllowDowngrade;
}

bool TestcaseSetup::dupAllowNameChange() const
{
  return m_dupAllowNameChange;
}

bool TestcaseSetup::dupAllowArchChange() const
{
  return m_dupAllowArchChange;
}

bool TestcaseSetup::dupAllowVendorChange() const
{
  return m_dupAllowVendorChange;
}

bool TestcaseSetup::showMediaId() const
{
  return m_showMediaId;
}

bool TestcaseSetup::setLicencebit() const
{
  return m_licencebit;
}

QString TestcaseSetup::resolverFocusAsString() const
{
  return QString::fromStdString( zypp::asString( m_resolverFocus ) );
}

QString TestcaseSetup::hardwareInfo() const
{
  return m_hardwareInfo;
}

QStringList TestcaseSetup::modaliaslist() const
{
  return m_modaliaslist.toList();
}

QStringList TestcaseSetup::multiversionspec() const
{
  return m_multiversionspec.toList();
}

QStringList TestcaseSetup::autoInstall() const
{
  return m_autoInstall;
}

QQmlListProperty<Channel> TestcaseSetup::channels()
{
  return QQmlListProperty<Channel>(this, this,
    &TestcaseSetup::qmlAppendListElement<Channel, &TestcaseSetup::addChannel>,
    &TestcaseSetup::qmlListCount<Channel, &TestcaseSetup::m_channels>,
    &TestcaseSetup::qmlListElement<Channel, &TestcaseSetup::m_channels>,
    nullptr);
}

QQmlListProperty<ForceInstall> TestcaseSetup::forceInstall()
{
  return QQmlListProperty<ForceInstall>(this, this,
    &TestcaseSetup::qmlAppendListElement<ForceInstall, &TestcaseSetup::addForceInstall>,
    &TestcaseSetup::qmlListCount<ForceInstall, &TestcaseSetup::m_forceInstall>,
    &TestcaseSetup::qmlListElement<ForceInstall, &TestcaseSetup::m_forceInstall>,
    nullptr);
}

QQmlListProperty<Source> TestcaseSetup::sources()
{
  return QQmlListProperty<Source>(this, this,
    &TestcaseSetup::qmlAppendListElement<Source, &TestcaseSetup::addSource>,
    &TestcaseSetup::qmlListCount<Source, &TestcaseSetup::m_sources>,
    &TestcaseSetup::qmlListElement<Source, &TestcaseSetup::m_sources>,
    nullptr);
}

QQmlListProperty<LocaleChange > TestcaseSetup::locales()
{
  return QQmlListProperty<LocaleChange>(this, this,
    &TestcaseSetup::qmlAppendListElement<LocaleChange, &TestcaseSetup::addLocale>,
    &TestcaseSetup::qmlListCount<LocaleChange, &TestcaseSetup::m_locales>,
    &TestcaseSetup::qmlListElement<LocaleChange, &TestcaseSetup::m_locales>,
    nullptr);
}

QString TestcaseSetup::arch() const
{
  return m_arch;
}

QString TestcaseSetup::systemCheck() const
{
  return m_systemCheck;
}

QStringList TestcaseSetup::resolverFocusModes() const
{
  return { QString::fromStdString( zypp::asString( zypp::ResolverFocus::Default)),
           QString::fromStdString( zypp::asString( zypp::ResolverFocus::Job)),
           QString::fromStdString( zypp::asString( zypp::ResolverFocus::Update)),
           QString::fromStdString( zypp::asString( zypp::ResolverFocus::Installed))
  };
}

void TestcaseSetup::setIgnorealreadyrecommended(bool ignorealreadyrecommended)
{
  if (m_ignorealreadyrecommended == ignorealreadyrecommended)
    return;

  m_ignorealreadyrecommended = ignorealreadyrecommended;
  emit ignorealreadyrecommendedChanged(m_ignorealreadyrecommended);
}

void TestcaseSetup::setOnlyRequires(bool onlyRequires)
{
  qDebug()<<"Called only requires with " << onlyRequires;
  if (m_onlyRequires == onlyRequires)
    return;

  m_onlyRequires = onlyRequires;
  emit onlyRequiresChanged(m_onlyRequires);
}

void TestcaseSetup::setForceResolve(bool forceResolve)
{
  if (m_forceResolve == forceResolve)
    return;

  m_forceResolve = forceResolve;
  emit forceResolveChanged(m_forceResolve);
}

void TestcaseSetup::setCleandepsOnRemove(bool cleandepsOnRemove)
{
  if (m_cleandepsOnRemove == cleandepsOnRemove)
    return;

  m_cleandepsOnRemove = cleandepsOnRemove;
  emit cleandepsOnRemoveChanged(m_cleandepsOnRemove);
}

void TestcaseSetup::setAllowDowngrade(bool allowDowngrade)
{
  if (m_allowDowngrade == allowDowngrade)
    return;

  m_allowDowngrade = allowDowngrade;
  emit allowDowngradeChanged(m_allowDowngrade);
}

void TestcaseSetup::setAllowNameChange(bool allowNameChange)
{
  if (m_allowNameChange == allowNameChange)
    return;

  m_allowNameChange = allowNameChange;
  emit allowNameChangeChanged(m_allowNameChange);
}

void TestcaseSetup::setAllowArchChange(bool allowArchChange)
{
  if (m_allowArchChange == allowArchChange)
    return;

  m_allowArchChange = allowArchChange;
  emit allowArchChangeChanged(m_allowArchChange);
}

void TestcaseSetup::setAllowVendorChange(bool allowVendorChange)
{
  if (m_allowVendorChange == allowVendorChange)
    return;

  m_allowVendorChange = allowVendorChange;
  emit allowVendorChangeChanged(m_allowVendorChange);
}

void TestcaseSetup::setDupAllowDowngrade(bool dupAllowDowngrade)
{
  if (m_dupAllowDowngrade == dupAllowDowngrade)
    return;

  m_dupAllowDowngrade = dupAllowDowngrade;
  emit dupAllowDowngradeChanged(m_dupAllowDowngrade);
}

void TestcaseSetup::setDupAllowNameChange(bool dupAllowNameChange)
{
  if (m_dupAllowNameChange == dupAllowNameChange)
    return;

  m_dupAllowNameChange = dupAllowNameChange;
  emit dupAllowNameChangeChanged(m_dupAllowNameChange);
}

void TestcaseSetup::setDupAllowArchChange(bool dupAllowArchChange)
{
  if (m_dupAllowArchChange == dupAllowArchChange)
    return;

  m_dupAllowArchChange = dupAllowArchChange;
  emit dupAllowArchChangeChanged(m_dupAllowArchChange);
}

void TestcaseSetup::setDupAllowVendorChange(bool dupAllowVendorChange)
{
  if (m_dupAllowVendorChange == dupAllowVendorChange)
    return;

  m_dupAllowVendorChange = dupAllowVendorChange;
  emit dupAllowVendorChangeChanged(m_dupAllowVendorChange);
}

void TestcaseSetup::setResolverFocusFromString(const QString &resolverFocus)
{
  zypp::ResolverFocus foc = zypp::ResolverFocus::Default;
  if ( !zypp::fromString( resolverFocus.toStdString(), foc ) ) {
    qWarning()<<"Invalid resolver focus string " << resolverFocus;
    return;
  }

  if (m_resolverFocus == foc)
    return;

  m_resolverFocus = foc;
  emit resolverFocusChanged( resolverFocus );
}

void TestcaseSetup::setHardwareInfo(QString hardwareInfo)
{
  if (m_hardwareInfo == hardwareInfo)
    return;

  m_hardwareInfo = hardwareInfo;
  emit hardwareInfoChanged(m_hardwareInfo);
}

void TestcaseSetup::addModalias( const QString &modalias )
{
  if (m_modaliaslist.contains(modalias))
    return;

  m_modaliaslist.insert(modalias);
  emit modaliaslistChanged();
}

void TestcaseSetup::addMultiversion(const QString &spec)
{
  if (m_multiversionspec.contains(spec))
    return;

  m_multiversionspec.insert(spec);
  emit multiversionspecChanged();
}

void TestcaseSetup::setShowMediaId(bool showMediaId)
{
  if (m_showMediaId == showMediaId)
    return;

  m_showMediaId = showMediaId;
  emit showMediaIdChanged(m_showMediaId);
}

void TestcaseSetup::setLicencebit(bool licencebit)
{
  if (m_licencebit == licencebit)
    return;

  m_licencebit = licencebit;
  emit licencebitChanged(m_licencebit);
}

void TestcaseSetup::setAutoInstall(QStringList autoInstall)
{
  if (m_autoInstall == autoInstall)
    return;

  m_autoInstall = autoInstall;
  emit autoInstallChanged(m_autoInstall);
}

void TestcaseSetup::addAutoInstall(const QString &autoInstall)
{
  m_autoInstall.push_back( autoInstall );
  emit autoInstallChanged( m_autoInstall );
}

void TestcaseSetup::addChannel( Channel *channel)
{
  if ( m_channels.contains( channel ) )
    return;

  channel->setParent( this );
  m_channels.push_back( channel );
  emit channelsChanged();
}

void TestcaseSetup::addForceInstall(ForceInstall *fi)
{
  if ( m_forceInstall.contains( fi ) )
    return;

  fi->setParent( this );
  m_forceInstall.push_back( fi );
  emit forceInstallChanged( );
}

void TestcaseSetup::addSource(Source *src)
{
  if ( m_sources.contains( src ) )
    return;

  src->setParent( this );
  m_sources.push_back( src );
  emit sourcesChanged();
}

void TestcaseSetup::addLocale(LocaleChange *loc)
{
  if ( m_locales.contains( loc ) )
    return;

  loc->setParent( this );
  m_locales.push_back( loc );
  emit localesChanged();
}

void TestcaseSetup::setArch(QString arch)
{
  if (m_arch == arch)
    return;

  m_arch = arch;
  emit archChanged(m_arch);
}

void TestcaseSetup::setSystemCheck(QString systemCheck)
{
  if (m_systemCheck == systemCheck)
    return;

  m_systemCheck = systemCheck;
  emit systemCheckChanged(m_systemCheck);
}
