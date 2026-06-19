#include <scwx/qt/ui/import/select_file_page.hpp>
#include <scwx/qt/types/settings_types.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <set>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QToolButton>

namespace scwx::qt::ui::import
{

static const std::string logPrefix_ = "scwx::qt::ui::import::select_file_page";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class SelectFilePage::Impl
{
public:
   explicit Impl(SelectFilePage* self) : self_ {self} {};
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void SelectFile();

   SelectFilePage* self_;

   QLayout* layout_ {};

   QLabel*      selectFileLabel_ {};
   QLineEdit*   selectFileLineEdit_ {};
   QToolButton* selectFileToolButton_ {};

   std::shared_ptr<zip::ZipStreamReader> settingsFile_ {};
   std::set<types::SettingsType>         settingsTypes_ {};
};

SelectFilePage::SelectFilePage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>(this)}
{
   // QObjects are managed by the parent
   // NOLINTBEGIN(cppcoreguidelines-owning-memory)

   p->selectFileLabel_      = new QLabel(this);
   p->selectFileLineEdit_   = new QLineEdit(this);
   p->selectFileToolButton_ = new QToolButton(this);

   p->selectFileLabel_->setText(tr("Import from:"));
   p->selectFileToolButton_->setText(tr("..."));

   // Overall layout
   p->layout_ = new QHBoxLayout(this);
   p->layout_->addWidget(p->selectFileLabel_);
   p->layout_->addWidget(p->selectFileLineEdit_);
   p->layout_->addWidget(p->selectFileToolButton_);
   setLayout(p->layout_);

   // NOLINTEND(cppcoreguidelines-owning-memory)

   // Connect signals
   connect(p->selectFileLineEdit_,
           &QLineEdit::textChanged,
           this,
           &QWizardPage::completeChanged);
   connect(p->selectFileToolButton_,
           &QAbstractButton::clicked,
           this,
           [this]() { p->SelectFile(); });
}

SelectFilePage::~SelectFilePage() = default;

bool SelectFilePage::isComplete() const
{
   return !p->selectFileLineEdit_->text().isEmpty();
}

bool SelectFilePage::validatePage()
{
   const QString title {"Import Settings"};
   const auto    importFilename = p->selectFileLineEdit_->text().toStdString();

   // Reset settings info
   p->settingsFile_ = nullptr;
   p->settingsTypes_.clear();

   try
   {
      if (!std::filesystem::exists(importFilename))
      {
         // File does not exist
         logger_->warn("File does not exist: {}", importFilename);
         QMessageBox::critical(
            this, title, tr("File does not exist:\n\n%1").arg(importFilename));
         return false;
      }

      if (std::filesystem::is_directory(importFilename))
      {
         // File is a directory
         logger_->warn("Selection was not a valid file: {}", importFilename);
         QMessageBox::critical(
            this,
            title,
            tr("Selection was not a valid file:\n\n%1").arg(importFilename));
         return false;
      }

      auto settingsFile =
         std::make_shared<zip::ZipStreamReader>(importFilename);
      if (!settingsFile->IsOpen())
      {
         // File is not a valid zip archive
         logger_->warn("File is not a valid zip archive: {}", importFilename);
         QMessageBox::critical(
            this,
            title,
            tr("File is not a valid ZIP archive:\n\n%1").arg(importFilename));
         return false;
      }

      std::set<types::SettingsType> settingsTypes {};
      const auto                    fileList = settingsFile->ListFiles();
      for (const auto& filename : fileList)
      {
         // Search for settings files
         const auto settingsType = types::GetSettingsTypeFromFilename(filename);
         if (settingsType != types::SettingsType::Unknown)
         {
            settingsTypes.emplace(settingsType);
         }
      }

      if (!settingsTypes.empty())
      {
         // Store settings info
         p->settingsFile_ = settingsFile;
         p->settingsTypes_.swap(settingsTypes);
      }
      else
      {
         logger_->warn("Archive does not contain any settings: {}",
                       importFilename);
         QMessageBox::critical(
            this,
            title,
            tr("Archive does not contain any settings:\n\n%1")
               .arg(importFilename));
         return false;
      }
   }
   catch (const std::exception& ex)
   {
      logger_->error("Error opening file: {}, {}", importFilename, ex.what());
      QMessageBox::critical(
         this, title, tr("Error opening file:\n\n%1").arg(importFilename));
      return false;
   }

   Q_EMIT FileSelected(importFilename, p->settingsFile_);

   return true;
}

void SelectFilePage::Impl::SelectFile()
{
   static const std::string zipFilter = "ZIP Archive (*.zip)";
   static const std::string allFilter = "All Files (*)";

   auto dialog = new QFileDialog(self_);

   dialog->setFileMode(QFileDialog::FileMode::AnyFile);
   dialog->setNameFilters(
      {QObject::tr(zipFilter.c_str()), QObject::tr(allFilter.c_str())});
   dialog->setAttribute(Qt::WA_DeleteOnClose);

   QObject::connect(dialog,
                    &QFileDialog::fileSelected,
                    self_,
                    [this](const QString& file)
                    {
                       // Use native directory separators
                       const QString filePath = QDir::toNativeSeparators(file);

                       logger_->debug("Selected: {}", filePath.toStdString());
                       selectFileLineEdit_->setText(filePath);
                    });

   dialog->open();
}

} // namespace scwx::qt::ui::import
