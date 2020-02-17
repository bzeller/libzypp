#ifndef TESTCASESETUP_H
#define TESTCASESETUP_H

#include <QObject>
#include <QString>
#include <QSet>
#include <QMetaType>
#include <QList>
#include <QVariantMap>
#include <QVariantList>
#include <zypp/ResolverFocus.h>
#include <zypp/ZConfig.h>
#include <QQmlListProperty>

class Channel;
class Source;
class ForceInstall;
class LocaleChange;

Q_DECLARE_METATYPE( zypp::ResolverFocus );

class TestcaseSetup : public QObject
{
  Q_OBJECT
  Q_PROPERTY(bool ignorealreadyrecommended READ ignorealreadyrecommended WRITE setIgnorealreadyrecommended NOTIFY ignorealreadyrecommendedChanged)
  Q_PROPERTY(bool onlyRequires READ onlyRequires WRITE setOnlyRequires NOTIFY onlyRequiresChanged)
  Q_PROPERTY(bool forceResolve READ forceResolve WRITE setForceResolve NOTIFY forceResolveChanged)
  Q_PROPERTY(bool cleandepsOnRemove READ cleandepsOnRemove WRITE setCleandepsOnRemove NOTIFY cleandepsOnRemoveChanged)
  Q_PROPERTY(bool allowDowngrade READ allowDowngrade WRITE setAllowDowngrade NOTIFY allowDowngradeChanged)
  Q_PROPERTY(bool allowNameChange READ allowNameChange WRITE setAllowNameChange NOTIFY allowNameChangeChanged)
  Q_PROPERTY(bool allowArchChange READ allowArchChange WRITE setAllowArchChange NOTIFY allowArchChangeChanged)
  Q_PROPERTY(bool allowVendorChange READ allowVendorChange WRITE setAllowVendorChange NOTIFY allowVendorChangeChanged)
  Q_PROPERTY(bool dupAllowDowngrade READ dupAllowDowngrade WRITE setDupAllowDowngrade NOTIFY dupAllowDowngradeChanged)
  Q_PROPERTY(bool dupAllowNameChange READ dupAllowNameChange WRITE setDupAllowNameChange NOTIFY dupAllowNameChangeChanged)
  Q_PROPERTY(bool dupAllowArchChange READ dupAllowArchChange WRITE setDupAllowArchChange NOTIFY dupAllowArchChangeChanged)
  Q_PROPERTY(bool dupAllowVendorChange READ dupAllowVendorChange WRITE setDupAllowVendorChange NOTIFY dupAllowVendorChangeChanged)
  Q_PROPERTY(bool showMediaId READ showMediaId WRITE setShowMediaId NOTIFY showMediaIdChanged)
  Q_PROPERTY(bool licencebit READ setLicencebit WRITE setLicencebit NOTIFY licencebitChanged)
  Q_PROPERTY(QString resolverFocus READ resolverFocusAsString WRITE setResolverFocusFromString NOTIFY resolverFocusChanged)
  Q_PROPERTY(QString hardwareInfo READ hardwareInfo WRITE setHardwareInfo NOTIFY hardwareInfoChanged)
  Q_PROPERTY(QString arch READ arch WRITE setArch NOTIFY archChanged)
  Q_PROPERTY(QString systemCheck READ systemCheck WRITE setSystemCheck NOTIFY systemCheckChanged)
  Q_PROPERTY(QStringList modaliaslist READ modaliaslist NOTIFY modaliaslistChanged)
  Q_PROPERTY(QStringList multiversionspec READ multiversionspec NOTIFY multiversionspecChanged)
  Q_PROPERTY(QStringList autoInstall READ autoInstall WRITE setAutoInstall NOTIFY autoInstallChanged)
  Q_PROPERTY(QQmlListProperty<Channel> channels READ channels )
  Q_PROPERTY(QQmlListProperty<ForceInstall> forceInstall READ forceInstall )
  Q_PROPERTY(QQmlListProperty<Source> sources READ sources )
  Q_PROPERTY(QQmlListProperty<LocaleChange> locales READ locales )
public:
  explicit TestcaseSetup(QObject *parent = nullptr);

  bool ignorealreadyrecommended() const;
  bool onlyRequires() const;
  bool forceResolve() const;
  bool cleandepsOnRemove() const;
  bool allowDowngrade() const;
  bool allowNameChange() const;
  bool allowArchChange() const;
  bool allowVendorChange() const;
  bool dupAllowDowngrade() const;
  bool dupAllowNameChange() const;
  bool dupAllowArchChange() const;
  bool dupAllowVendorChange() const;
  bool showMediaId() const;
  bool setLicencebit() const;
  QString resolverFocusAsString() const;
  QString hardwareInfo() const;
  QStringList modaliaslist() const;
  QStringList multiversionspec() const;
  QStringList localesToAdd() const;
  QStringList localesToRem() const;
  QStringList autoInstall() const;
  QQmlListProperty<Channel> channels();
  QQmlListProperty<ForceInstall> forceInstall();
  QQmlListProperty<Source> sources();
  QQmlListProperty<LocaleChange> locales();
  QString arch() const;
  QString systemCheck() const;

  Q_INVOKABLE QStringList resolverFocusModes() const;

public slots:
  bool loadTestcase(const QString &fileName );
  void setIgnorealreadyrecommended(bool ignorealreadyrecommended);
  void setOnlyRequires(bool onlyRequires);
  void setForceResolve(bool forceResolve);
  void setCleandepsOnRemove(bool cleandepsOnRemove);
  void setAllowDowngrade(bool allowDowngrade);
  void setAllowNameChange(bool allowNameChange);
  void setAllowArchChange(bool allowArchChange);
  void setAllowVendorChange(bool allowVendorChange);
  void setDupAllowDowngrade(bool dupAllowDowngrade);
  void setDupAllowNameChange(bool dupAllowNameChange);
  void setDupAllowArchChange(bool dupAllowArchChange);
  void setDupAllowVendorChange(bool dupAllowVendorChange);
  void setResolverFocusFromString(const QString &resolverFocus);
  void setHardwareInfo(QString hardwareInfo);
  void addModalias(const QString &modalias);
  void addMultiversion(const QString &spec);
  void setShowMediaId(bool showMediaId);
  void setLicencebit(bool licencebit);
  void setAutoInstall(QStringList autoInstall);
  void addAutoInstall( const QString &autoInstall );
  void addChannel( Channel *channel );
  void addForceInstall( ForceInstall *fi );
  void addSource( Source *src );
  void addLocale( LocaleChange *loc );
  void setArch(QString arch);
  void setSystemCheck(QString systemCheck);

signals:
  void ignorealreadyrecommendedChanged(bool ignorealreadyrecommended);
  void onlyRequiresChanged(bool onlyRequires);
  void forceResolveChanged(bool forceResolve);
  void cleandepsOnRemoveChanged(bool cleandepsOnRemove);
  void allowDowngradeChanged(bool allowDowngrade);
  void allowNameChangeChanged(bool allowNameChange);
  void allowArchChangeChanged(bool allowArchChange);
  void allowVendorChangeChanged(bool allowVendorChange);
  void dupAllowDowngradeChanged(bool dupAllowDowngrade);
  void dupAllowNameChangeChanged(bool dupAllowNameChange);
  void dupAllowArchChangeChanged(bool dupAllowArchChange);
  void dupAllowVendorChangeChanged(bool dupAllowVendorChange);
  void showMediaIdChanged(bool showMediaId);
  void setLicenceChanged(bool setLicence);
  void licencebitChanged(bool licencebit);
  void resolverFocusChanged(QString resolverFocus);
  void hardwareInfoChanged(QString hardwareInfo);
  void autoInstallChanged(QStringList autoInstall);
  void archChanged(QString arch);
  void systemCheckChanged(QString systemCheck);
  void modaliaslistChanged();
  void multiversionspecChanged();
  void channelsChanged();
  void forceInstallChanged( );
  void sourcesChanged( );
  void localesChanged( );

private:
  template< typename T, void (TestcaseSetup::*callback)( T* ) >
  static void qmlAppendListElement(QQmlListProperty<T>* list, T* obj ) {
    ((reinterpret_cast< TestcaseSetup* >(list->data))->*callback)( obj );
  }

  template< typename T, QList<T *> TestcaseSetup::* member >
  static int qmlListCount(QQmlListProperty<T>* list) {
    const auto &listRef = reinterpret_cast< TestcaseSetup* >(list->data)->*member;
    return listRef.count();
  }

  template< typename T, QList<T *> TestcaseSetup::* member >
  static T* qmlListElement(QQmlListProperty<T>* list, int index ) {
    const auto &listRef = reinterpret_cast< TestcaseSetup* >(list->data)->*member;
    if ( index < 0 || index >= listRef.count() )
      return nullptr;
    return listRef.at(index);
  }

private:
  bool m_ignorealreadyrecommended = false;
  bool m_onlyRequires = false;
  bool m_forceResolve = false;
  bool m_cleandepsOnRemove = false;
  bool m_allowDowngrade = false;
  bool m_allowNameChange = false;
  bool m_allowArchChange = false;
  bool m_allowVendorChange = false;
  bool m_dupAllowDowngrade = false;
  bool m_dupAllowNameChange = false;
  bool m_dupAllowArchChange = false;
  bool m_dupAllowVendorChange = false;
  bool m_showMediaId = false;
  bool m_setLicence = false;
  bool m_licencebit = false;
  zypp::ResolverFocus m_resolverFocus = zypp::ResolverFocus::Default;
  QString m_hardwareInfo;
  QString m_systemRepo;
  QStringList m_autoInstall;
  QSet<QString> m_modaliaslist;
  QSet<QString> m_multiversionspec;
  QList<Channel *> m_channels;
  QList<ForceInstall *> m_forceInstall;
  QList<Source *> m_sources;
  QList<LocaleChange *> m_locales;
  QString m_arch;
  QString m_systemCheck;
};

#endif // TESTCASESETUP_H
