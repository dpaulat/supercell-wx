#include <scwx/qt/ui/setup/map_layout_page.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <boost/multi_array.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

static constexpr std::size_t kGridWidth_ {2u};
static constexpr std::size_t kGridHeight_ {2u};

class MapLayoutPage::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   void SetupSettingsInterface();
   void UpdateGridDisplay();

   QLayout* layout_ {};

   QLabel*      descriptionLabel_ {};
   QFrame*      settingsFrame_ {};
   QGridLayout* settingsLayout_ {};
   QSpacerItem* settingsSpacer_ {};
   QLabel*      gridWidthLabel_ {};
   QSpinBox*    gridWidthSpinBox_ {};
   QLabel*      gridHeightLabel_ {};
   QSpinBox*    gridHeightSpinBox_ {};

   QSpacerItem*                             leftGridSpacer_ {};
   QSpacerItem*                             rightGridSpacer_ {};
   QFrame*                                  gridFrame_ {};
   QGridLayout*                             gridLayout_ {};
   boost::multi_array<QFrame*, kGridWidth_> gridPanes_ {
      boost::extents[kGridWidth_][kGridHeight_]};

   settings::SettingsInterface<std::int64_t> gridWidth_ {};
   settings::SettingsInterface<std::int64_t> gridHeight_ {};
};

MapLayoutPage::MapLayoutPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
   setTitle(tr("Map Layout"));
   setSubTitle(tr("Configure the Supercell Wx map layout."));

   p->descriptionLabel_  = new QLabel(this);
   p->settingsFrame_     = new QFrame(this);
   p->settingsLayout_    = new QGridLayout(p->settingsFrame_);
   p->gridWidthLabel_    = new QLabel(this);
   p->gridWidthSpinBox_  = new QSpinBox(this);
   p->gridHeightLabel_   = new QLabel(this);
   p->gridHeightSpinBox_ = new QSpinBox(this);

   p->settingsSpacer_ =
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
   p->leftGridSpacer_ =
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
   p->rightGridSpacer_ =
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
   p->gridFrame_  = new QFrame(this);
   p->gridLayout_ = new QGridLayout(p->gridFrame_);

   // Description
   p->descriptionLabel_->setText(
      tr("Grid Width and Grid Height settings allow multiple radar products to "
         "be viewed simultaneously. Changing Grid Width will create panes "
         "side by side, while Grid Height will create panes over and under."));
   p->descriptionLabel_->setWordWrap(true);

   // Settings
   p->gridWidthLabel_->setText(tr("Grid Width"));
   p->gridHeightLabel_->setText(tr("Grid Height"));
   p->gridWidthSpinBox_->setMinimumWidth(100);
   p->gridHeightSpinBox_->setMinimumWidth(100);
   p->settingsLayout_->setContentsMargins(0, 0, 0, 0);
   p->settingsLayout_->addWidget(p->gridWidthLabel_, 0, 0);
   p->settingsLayout_->addWidget(p->gridWidthSpinBox_, 0, 1);
   p->settingsLayout_->addWidget(p->gridHeightLabel_, 1, 0);
   p->settingsLayout_->addWidget(p->gridHeightSpinBox_, 1, 1);
   p->settingsLayout_->addItem(p->settingsSpacer_, 0, 2);
   p->settingsFrame_->setLayout(p->settingsLayout_);

   // Grid
   p->gridLayout_->setContentsMargins(0, 0, 0, 0);
   p->gridLayout_->setSpacing(1);
   p->gridLayout_->addItem(p->leftGridSpacer_, 0, 0);
   p->gridLayout_->addItem(p->rightGridSpacer_, 0, kGridWidth_ + 1);
   for (std::size_t i = 0; i < kGridWidth_; ++i)
   {
      for (std::size_t j = 0; j < kGridHeight_; ++j)
      {
         auto& pane = p->gridPanes_[i][j];
         pane       = new QFrame(this);
         pane->setStyleSheet("background-color:black;");
         pane->setFixedSize(75, 50);
         p->gridLayout_->addWidget(
            pane, static_cast<int>(j), static_cast<int>(i + 1));
      }
   }

   // Overall layout
   p->layout_ = new QVBoxLayout(this);
   p->layout_->addWidget(p->descriptionLabel_);
   p->layout_->addWidget(p->settingsFrame_);
   p->layout_->addWidget(p->gridFrame_);
   setLayout(p->layout_);

   // Configure settings interface
   p->SetupSettingsInterface();

   // Connect signals
   connect(p->gridWidthSpinBox_,
           &QSpinBox::valueChanged,
           this,
           [this]() { p->UpdateGridDisplay(); });
   connect(p->gridHeightSpinBox_,
           &QSpinBox::valueChanged,
           this,
           [this]() { p->UpdateGridDisplay(); });

   // Update grid display
   p->UpdateGridDisplay();
}

MapLayoutPage::~MapLayoutPage() = default;

void MapLayoutPage::Impl::SetupSettingsInterface()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   gridWidth_.SetSettingsVariable(generalSettings.grid_width());
   gridWidth_.SetEditWidget(gridWidthSpinBox_);

   gridHeight_.SetSettingsVariable(generalSettings.grid_height());
   gridHeight_.SetEditWidget(gridHeightSpinBox_);
}

void MapLayoutPage::Impl::UpdateGridDisplay()
{
   for (std::size_t i = 0; i < kGridWidth_; ++i)
   {
      for (std::size_t j = 0; j < kGridHeight_; ++j)
      {
         gridPanes_[i][j]->setVisible(
            static_cast<int>(i) < gridWidthSpinBox_->value() &&
            static_cast<int>(j) < gridHeightSpinBox_->value());
      }
   }
}

bool MapLayoutPage::validatePage()
{
   bool committed = false;

   committed |= p->gridWidth_.Commit();
   committed |= p->gridHeight_.Commit();

   if (committed)
   {
      manager::SettingsManager::Instance().SaveSettings();
   }

   return true;
}

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
