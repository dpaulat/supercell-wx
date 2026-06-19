#include "import_settings_wizard.hpp"

#include <scwx/qt/types/settings_types.hpp>
#include <scwx/qt/ui/import/import_options_page.hpp>
#include <scwx/qt/ui/import/select_file_page.hpp>
#include <scwx/util/logger.hpp>

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

namespace scwx::qt::ui::import
{

static const std::string logPrefix_ =
   "scwx::qt::ui::import::import_settings_wizard";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

class ImportSettingsWizard::Impl
{
public:
   explicit Impl(ImportSettingsWizard* self) : self_ {self} {}
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void ConnectSignals();

   ImportSettingsWizard* self_;

   ImportOptionsPage* importOptionsPage_ {};
   SelectFilePage*    selectFilePage_ {};
};

ImportSettingsWizard::ImportSettingsWizard(QWidget* parent) :
    QWizard(parent), p {std::make_unique<Impl>(this)}
{
   static constexpr int kWidth  = 400;
   static constexpr int kHeight = 250;

   resize(kWidth, kHeight);

   setWindowTitle(tr("Import Settings"));
   setWizardStyle(QWizard::WizardStyle::ClassicStyle);

   // QObjects are managed by the parent
   // NOLINTBEGIN(cppcoreguidelines-owning-memory)

   p->selectFilePage_    = new SelectFilePage(this);
   p->importOptionsPage_ = new ImportOptionsPage(this);

   // NOLINTEND(cppcoreguidelines-owning-memory)

   setPage(0, p->selectFilePage_);
   setPage(1, p->importOptionsPage_);

   p->ConnectSignals();
}

ImportSettingsWizard::~ImportSettingsWizard() = default;

void ImportSettingsWizard::Impl::ConnectSignals()
{
   QObject::connect(selectFilePage_,
                    &SelectFilePage::FileSelected,
                    importOptionsPage_,
                    &ImportOptionsPage::set_settings_file);
}

} // namespace scwx::qt::ui::import
