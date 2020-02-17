#include "mainwindow.h"
#include "testcase/testcasesetup.h"
#include "ui_mainwindow.h"

#include <zypp/base/LogControl.h>

#include <QActionGroup>
#include <QQmlEngine>
#include <QQmlContext>


MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow),
  m_setup( new TestcaseSetup(this) )
{
  ui->setupUi(this);
  modesGroup = new QActionGroup( this );
  modesGroup->addAction( ui->actionTestcase );
  modesGroup->addAction( ui->actionPool );
  connect( modesGroup, &QActionGroup::triggered, this, &MainWindow::modeActionTriggered );

  ui->quickWidget->engine()->rootContext()->setContextProperty("testcaseData", m_setup);
  ui->quickWidget->setSource(QStringLiteral("qrc:/TestcaseSettingsView.qml"));

  //show the testcase UI
  ui->actionTestcase->trigger();



}

MainWindow::~MainWindow()
{
  delete ui;
}

void MainWindow::modeActionTriggered ( QAction *triggered )
{
  int mode = modesGroup->actions().indexOf( triggered );
  if ( mode == -1 )
    return;

}
