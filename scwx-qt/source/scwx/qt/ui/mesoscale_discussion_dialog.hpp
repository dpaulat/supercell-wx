#pragma once

#include <scwx/spc/spc_md_types.hpp>
#include <scwx/common/geographic.hpp>

#include <memory>

#include <QDialog>

namespace Ui
{
class MesoscaleDiscussionDialog;
}

namespace scwx::qt::ui
{

class MesoscaleDiscussionDialogImpl;

class MesoscaleDiscussionDialog : public QDialog
{
   Q_OBJECT

public:
   explicit MesoscaleDiscussionDialog(QWidget* parent = nullptr);
   ~MesoscaleDiscussionDialog();

   bool SelectDiscussion(int mdNumber);

public slots:
   void on_firstButton_clicked();
   void on_previousButton_clicked();
   void on_nextButton_clicked();
   void on_lastButton_clicked();

signals:
   void MoveMap(double latitude, double longitude);

private:
   friend class MesoscaleDiscussionDialogImpl;
   std::unique_ptr<MesoscaleDiscussionDialogImpl> p;
   Ui::MesoscaleDiscussionDialog*                 ui;
};

} // namespace scwx::qt::ui
