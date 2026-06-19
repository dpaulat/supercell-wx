#include "export_settings_dialog.hpp"
#include "ui_export_settings_dialog.h"

#include <scwx/qt/types/settings_types.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/zip/zip_stream_writer.hpp>

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

namespace scwx::qt::ui
{

static const std::string logPrefix_ = "scwx::qt::ui::export_settings_dialog";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ExportSettingsDialog::Impl
{
public:
   explicit Impl(ExportSettingsDialog* self) : self_ {self} {}
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void ConnectSignals();
   void ExportSettings(const std::string& destination);
   void SelectFile();

   ExportSettingsDialog* self_;
};

ExportSettingsDialog::ExportSettingsDialog(QWidget* parent) :
    QDialog(parent),
    p {std::make_unique<Impl>(this)},
    ui(new Ui::ExportSettingsDialog)
{
   ui->setupUi(this);

   for (const auto type : types::SettingsTypeIterator())
   {
      const QString name =
         QString::fromStdString(types::GetSettingsTypeName(type));

      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): Owned by parent
      auto item = new QListWidgetItem(name, ui->settingsListWidget);
      item->setFlags(item->flags() | Qt::ItemFlag::ItemIsUserCheckable);
      item->setCheckState(Qt::CheckState::Checked);
      item->setData(Qt::ItemDataRole::UserRole, static_cast<int>(type));
   }

   ui->buttonBox->button(QDialogButtonBox::StandardButton::Ok)
      ->setEnabled(false);

   p->ConnectSignals();
}

ExportSettingsDialog::~ExportSettingsDialog()
{
   delete ui;
}

void ExportSettingsDialog::Impl::ConnectSignals()
{
   QObject::connect(self_->ui->destinationLineEdit,
                    &QLineEdit::textChanged,
                    self_,
                    [this](const QString& text)
                    {
                       self_->ui->buttonBox
                          ->button(QDialogButtonBox::StandardButton::Ok)
                          ->setEnabled(!text.trimmed().isEmpty());
                    });

   QObject::connect(self_->ui->destinationFileButton,
                    &QAbstractButton::clicked,
                    self_,
                    [this]() { SelectFile(); });

   QObject::connect(self_->ui->buttonBox,
                    &QDialogButtonBox::clicked,
                    self_,
                    [this](QAbstractButton* button)
                    {
                       const QDialogButtonBox::ButtonRole role =
                          self_->ui->buttonBox->buttonRole(button);

                       if (role != QDialogButtonBox::ButtonRole::AcceptRole)
                       {
                          // Not handling
                          return;
                       }

                       const QString destination =
                          self_->ui->destinationLineEdit->text().trimmed();
                       if (destination.isEmpty())
                       {
                          return;
                       }

                       const QFileInfo fileInfo {destination};
                       if (fileInfo.exists())
                       {
                          logger_->error("Overwrite file?");
                          const auto result = QMessageBox::question(
                             self_,
                             tr("Overwrite File?"),
                             tr("The file %1 already exists.\n\nOverwite it?")
                                .arg(destination),
                             QMessageBox::StandardButton::Yes |
                                QMessageBox::StandardButton::No,
                             QMessageBox::StandardButton::No);
                          logger_->error("Result");

                          if (result == QMessageBox::Yes)
                          {
                             // Proceed and close dialog
                             ExportSettings(destination.toStdString());
                          }
                          else
                          {
                             // Let the user change the destination
                             self_->ui->destinationLineEdit->setFocus();
                             self_->ui->destinationLineEdit->selectAll();
                          }
                       }
                       else
                       {
                          // File doesn't exist, proceed
                          ExportSettings(destination.toStdString());
                       }
                    });
}

void ExportSettingsDialog::Impl::SelectFile()
{
   static const std::string zipFilter = "ZIP Archive (*.zip)";

   auto dialog = new QFileDialog(self_);

   dialog->setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
   dialog->setFileMode(QFileDialog::FileMode::AnyFile);
   dialog->setNameFilter(QObject::tr(zipFilter.c_str()));
   dialog->setAttribute(Qt::WA_DeleteOnClose);

   QObject::connect(dialog,
                    &QFileDialog::fileSelected,
                    self_,
                    [this](const QString& file)
                    {
                       QString filePath = file;

                       // If no extension is provided, append .zip
                       const QFileInfo fileInfo {filePath};
                       if (fileInfo.suffix().isEmpty())
                       {
                          filePath += ".zip";
                       }

                       // Use native directory separators
                       filePath = QDir::toNativeSeparators(filePath);

                       logger_->debug("Selected: {}", filePath.toStdString());
                       self_->ui->destinationLineEdit->setText(filePath);
                    });

   dialog->open();
}

void ExportSettingsDialog::Impl::ExportSettings(const std::string& destination)
{
   zip::ZipStreamWriter zipStreamWriter(destination);
   if (!zipStreamWriter.IsOpen())
   {
      logger_->error("Unable to export settings");

      QMessageBox::critical(self_,
                            tr("Export Error"),
                            tr("Unable to export settings to %1.")
                               .arg(QString::fromStdString(destination)),
                            QMessageBox::StandardButton::Ok,
                            QMessageBox::StandardButton::Ok);
   }

   const QListWidget* listWidget = self_->ui->settingsListWidget;

   for (int i = 0; i < listWidget->count(); ++i)
   {
      const QListWidgetItem* item = listWidget->item(i);
      if (!item)
      {
         continue;
      }
      if (item->checkState() == Qt::CheckState::Checked)
      {
         const auto settingsType = static_cast<types::SettingsType>(
            item->data(Qt::ItemDataRole::UserRole).toInt());

         // Write settings to stream
         std::stringstream ss {};
         types::WriteSettingsFile(settingsType, ss);

         // Save settings
         const std::string& filename =
            types::GetSettingsTypeFilename(settingsType);
         zipStreamWriter.AddFile(filename, ss.str());
      }
   }

   zipStreamWriter.Close();

   self_->accept();
}

} // namespace scwx::qt::ui
