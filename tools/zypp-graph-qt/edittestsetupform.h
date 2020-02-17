#ifndef EDITTESTSETUPFORM_H
#define EDITTESTSETUPFORM_H

#include <QWidget>

namespace Ui {
class EditTestSetupForm;
}


class TestcaseSetup;

class EditTestSetupForm : public QWidget
{
  Q_OBJECT

public:
  explicit EditTestSetupForm(QWidget *parent = nullptr);
  ~EditTestSetupForm();

  void setTestcaseSetup ( TestcaseSetup *setup );

private:
  Ui::EditTestSetupForm *ui;
  TestcaseSetup *m_setup = nullptr;
};

#endif // EDITTESTSETUPFORM_H
