#pragma once

#include <scwx/qt/types/github_types.hpp>

#include <QDialog>

namespace Ui
{
class UpdateDialog;
}

namespace scwx
{
namespace qt
{
namespace ui
{

class UpdateDialogImpl;

class UpdateDialog : public QDialog
{
   Q_OBJECT

public:
   explicit UpdateDialog(QWidget* parent = nullptr);
   ~UpdateDialog();

   void UpdateReleaseInfo(const std::string&        latestVersion,
                          const types::gh::Release& latestRelease);

private slots:
   void on_downloadButton_clicked();

private:
   friend UpdateDialogImpl;
   std::unique_ptr<UpdateDialogImpl> p;
   Ui::UpdateDialog*                 ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
