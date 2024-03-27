#include "update_dialog.hpp"
#include "ui_update_dialog.h"
#include <scwx/qt/main/versions.hpp>
#include <scwx/qt/manager/download_manager.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/ui/download_dialog.hpp>
#include <scwx/util/logger.hpp>

#include <QDesktopServices>
#include <QFontDatabase>
#include <QProcess>
#include <QStandardPaths>

namespace scwx
{
namespace qt
{
namespace ui
{

static const std::string logPrefix_ = "scwx::qt::ui::update_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class UpdateDialog::Impl
{
public:
   explicit Impl(UpdateDialog* self) : self_ {self} {};
   ~Impl() = default;

   void HandleAsset(const types::gh::ReleaseAsset& asset);

   UpdateDialog* self_;

   std::shared_ptr<manager::DownloadManager> downloadManager_ {
      manager::DownloadManager::Instance()};

   std::string downloadUrl_ {};
   std::string installUrl_ {};
   std::string installFilename_ {};
};

UpdateDialog::UpdateDialog(QWidget* parent) :
    QDialog(parent), p {std::make_unique<Impl>(this)}, ui(new Ui::UpdateDialog)
{
   ui->setupUi(this);

   int titleFontId =
      manager::FontManager::Instance().GetFontId(types::Font::din1451alt_g);
   QString titleFontFamily =
      QFontDatabase::applicationFontFamilies(titleFontId).at(0);
   QFont titleFont(titleFontFamily, 12);
   ui->bannerLabel->setFont(titleFont);

   ui->releaseNotesText->setOpenExternalLinks(true);

   ui->installUpdateButton->setVisible(false);
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

   ui->installUpdateButton->setVisible(false);

   for (auto& asset : latestRelease.assets_)
   {
      p->HandleAsset(asset);
   }
}

void UpdateDialog::Impl::HandleAsset(const types::gh::ReleaseAsset& asset)
{
#if defined(_WIN32)
   if (asset.name_.ends_with(".msi"))
   {
      self_->ui->installUpdateButton->setVisible(true);
      installUrl_      = asset.browserDownloadUrl_;
      installFilename_ = asset.name_;
   }
#else
   Q_UNUSED(asset)
#endif
}

void UpdateDialog::on_downloadButton_clicked()
{
   if (!p->downloadUrl_.empty())
   {
      QDesktopServices::openUrl(QUrl {QString::fromStdString(p->downloadUrl_)});
   }
}

void UpdateDialog::on_installUpdateButton_clicked()
{
   if (!p->installUrl_.empty())
   {
      ui->installUpdateButton->setEnabled(false);

      std::string destinationPath {
         QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            .toStdString()};

      std::shared_ptr<request::DownloadRequest> request =
         std::make_shared<request::DownloadRequest>(
            p->installUrl_,
            std::filesystem::path(destinationPath) / p->installFilename_);

      DownloadDialog* downloadDialog = new DownloadDialog(this);
      downloadDialog->setAttribute(Qt::WA_DeleteOnClose);

      // Connect request signals
      connect(request.get(),
              &request::DownloadRequest::ProgressUpdated,
              downloadDialog,
              &DownloadDialog::UpdateProgress);
      connect(request.get(),
              &request::DownloadRequest::RequestComplete,
              downloadDialog,
              [=](request::DownloadRequest::CompleteReason reason)
              {
                 switch (reason)
                 {
                 case request::DownloadRequest::CompleteReason::OK:
                    downloadDialog->FinishDownload();
                    break;

                 default:
                    downloadDialog->CancelDownload();
                    break;
                 }
              });

      // Connect dialog signals
      connect(
         downloadDialog,
         &QDialog::accepted,
         this,
         [=, this]()
         {
            std::filesystem::path installerPackage =
               request->destination_path();
            installerPackage.make_preferred();

            logger_->info("Launching application installer: {}",
                          installerPackage.string());

            if (!QProcess::startDetached(
                   "msiexec.exe",
                   {"/i", QString::fromStdString(installerPackage.string())}))
            {
               logger_->error("Failed to launch installer");
            }

            ui->installUpdateButton->setEnabled(true);
         });
      connect(downloadDialog,
              &QDialog::rejected,
              this,
              [=, this]()
              {
                 request->Cancel();

                 ui->installUpdateButton->setEnabled(true);
              });

      downloadDialog->set_filename(p->installFilename_);
      downloadDialog->StartDownload();

      p->downloadManager_->Download(request);
   }
}

} // namespace ui
} // namespace qt
} // namespace scwx
