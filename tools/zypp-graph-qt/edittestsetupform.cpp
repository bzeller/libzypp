#include "edittestsetupform.h"
#include "testcasesetup.h"
#include "ui_edittestsetupform.h"

#include <zypp/ResolverFocus.h>
#include <QMetaObject>
#include <QDebug>
#include <QMetaProperty>
#include <QMetaMethod>

EditTestSetupForm::EditTestSetupForm(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::EditTestSetupForm)
{
  ui->setupUi(this);
  ui->comboBoxSolverFocus->addItem( QString::fromStdString( zypp::asString( zypp::ResolverFocus::Default ) ), (int) zypp::ResolverFocus::Default );
  ui->comboBoxSolverFocus->addItem( QString::fromStdString( zypp::asString( zypp::ResolverFocus::Job ) ), (int) zypp::ResolverFocus::Job );
  ui->comboBoxSolverFocus->addItem( QString::fromStdString( zypp::asString( zypp::ResolverFocus::Update ) ), (int) zypp::ResolverFocus::Update );
  ui->comboBoxSolverFocus->addItem( QString::fromStdString( zypp::asString( zypp::ResolverFocus::Installed ) ), (int) zypp::ResolverFocus::Installed );
}

EditTestSetupForm::~EditTestSetupForm()
{
  delete ui;
}

void EditTestSetupForm::setTestcaseSetup(TestcaseSetup *setup)
{
  Q_ASSERT( !m_setup );
  m_setup = setup;

  Q_PROPERTY(bool cleandepsOnRemove READ cleandepsOnRemove WRITE setCleandepsOnRemove NOTIFY cleandepsOnRemoveChanged)
  Q_PROPERTY(bool allowDowngrade READ allowDowngrade WRITE setAllowDowngrade NOTIFY allowDowngradeChanged)
  Q_PROPERTY(bool allowNameChange READ allowNameChange WRITE setAllowNameChange NOTIFY allowNameChangeChanged)
  Q_PROPERTY(bool allowArchChange READ allowArchChange WRITE setAllowArchChange NOTIFY allowArchChangeChanged)
  Q_PROPERTY(bool allowVendorChange READ allowVendorChange WRITE setAllowVendorChange NOTIFY allowVendorChangeChanged)
  Q_PROPERTY(bool dupAllowDowngrade READ dupAllowDowngrade WRITE setDupAllowDowngrade NOTIFY dupAllowDowngradeChanged)
  Q_PROPERTY(bool dupAllowNameChange READ dupAllowNameChange WRITE setDupAllowNameChange NOTIFY dupAllowNameChangeChanged)
  Q_PROPERTY(bool dupAllowArchChange READ dupAllowArchChange WRITE setDupAllowArchChange NOTIFY dupAllowArchChangeChanged)
  Q_PROPERTY(bool dupAllowVendorChange READ dupAllowVendorChange WRITE setDupAllowVendorChange NOTIFY dupAllowVendorChangeChanged)
  Q_PROPERTY(bool licencebit READ setLicencebit WRITE setLicencebit NOTIFY licencebitChanged)

  connect( ui->checkBoxIgnorealreadyrecommended, SIGNAL(toggled(bool)), m_setup, SLOT( setIgnorealreadyrecommended(bool) ));
  connect( m_setup, SIGNAL( ignorealreadyrecommendedChanged(bool) ), ui->checkBoxIgnorealreadyrecommended, SLOT(setChecked(bool)) );
  ui->checkBoxIgnorealreadyrecommended->setChecked( m_setup->ignorealreadyrecommended() );

  connect( ui->checkBoxOnlyRequires, SIGNAL(toggled(bool)), m_setup, SLOT( setOnlyRequires(bool) ));
  connect( m_setup, SIGNAL( onlyRequiresChanged(bool) ), ui->checkBoxOnlyRequires, SLOT(setChecked(bool)) );
  ui->checkBoxOnlyRequires->setChecked( m_setup->onlyRequires() );

  connect( ui->checkBoxForceResolve, SIGNAL(toggled(bool)), m_setup, SLOT( setForceResolve(bool) ));
  connect( m_setup, SIGNAL( forceResolveChanged(bool) ), ui->checkBoxForceResolve, SLOT(setChecked(bool)) );
  ui->checkBoxForceResolve->setChecked( m_setup->forceResolve() );

  connect( ui->checkBoxshowMediaId, SIGNAL(toggled(bool)), m_setup, SLOT(setShowMediaId(bool)));
  connect( m_setup, SIGNAL( showMediaIdChanged(bool) ), ui->checkBoxshowMediaId, SLOT(setChecked(bool)) );
  ui->checkBoxshowMediaId->setChecked( m_setup->showMediaId() );

}
