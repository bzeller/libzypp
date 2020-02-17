#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QActionGroup;
class TestcaseSetup;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void modeActionTriggered( QAction *triggered );

private:
  Ui::MainWindow *ui;
  TestcaseSetup *m_setup;
  QActionGroup *modesGroup;
};

#endif // MAINWINDOW_H
