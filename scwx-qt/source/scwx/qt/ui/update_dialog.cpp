#include "update_dialog.hpp"
#include "ui_update_dialog.h"
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/manager/resource_manager.hpp>

#include <QDesktopServices>
#include <QFontDatabase>

namespace scwx
{
namespace qt
{
namespace ui
{

class UpdateDialogImpl
{
public:
   explicit UpdateDialogImpl() = default;
   ~UpdateDialogImpl()         = default;

   std::string downloadUrl_ {};
};

UpdateDialog::UpdateDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<UpdateDialogImpl>()},
    ui(new Ui::UpdateDialog)
{
   ui->setupUi(this);

   int titleFontId =
      manager::ResourceManager::FontId(types::Font::din1451alt_g);
   QString titleFontFamily =
      QFontDatabase::applicationFontFamilies(titleFontId).at(0);
   QFont titleFont(titleFontFamily, 12);
   ui->bannerLabel->setFont(titleFont);

   ui->releaseNotesText->setOpenExternalLinks(true);
}

UpdateDialog::~UpdateDialog()
{
   delete ui;
}

void UpdateDialog::UpdateReleaseInfo(const std::string&        latestVersion,
                                     const types::gh::Release& latestRelease)
{
   ui->versionLabel->setText(tr("Supercell Wx v%1 is now available. You are "
                                "currently running version %2.")
                                .arg(latestVersion.c_str())
                                .arg(main::kVersionString_.c_str()));

   ui->releaseNotesText->setMarkdown(
      QString::fromStdString(latestRelease.body_));

   p->downloadUrl_ = latestRelease.htmlUrl_;
}

void UpdateDialog::on_downloadButton_clicked()
{
   if (!p->downloadUrl_.empty())
   {
      QDesktopServices::openUrl(QUrl {QString::fromStdString(p->downloadUrl_)});
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
