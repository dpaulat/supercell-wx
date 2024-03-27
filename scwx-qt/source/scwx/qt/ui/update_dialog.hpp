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

class UpdateDialog : public QDialog
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(UpdateDialog)

public:
   explicit UpdateDialog(QWidget* parent = nullptr);
   ~UpdateDialog();

   void UpdateReleaseInfo(const std::string&        latestVersion,
                          const types::gh::Release& latestRelease);

private slots:
   void on_downloadButton_clicked();
   void on_installUpdateButton_clicked();

private:
   class Impl;
   std::unique_ptr<Impl> p;
   Ui::UpdateDialog*     ui;
};

} // namespace ui
} // namespace qt
} // namespace scwx
