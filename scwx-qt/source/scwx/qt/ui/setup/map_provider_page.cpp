#include <scwx/qt/ui/setup/map_provider_page.hpp>
#include <scwx/qt/manager/settings_manager.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/settings/settings_interface.hpp>

#include <unordered_map>

#include <QComboBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <boost/algorithm/string.hpp>

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

static const std::unordered_map<map::MapProvider, QUrl> kUrl_ {
   {map::MapProvider::Mapbox, QUrl {"https://www.mapbox.com/"}},
   {map::MapProvider::MapTiler, QUrl {"https://www.maptiler.com/"}}};

struct MapProviderGroup
{
   QLabel*    apiKeyLabel_ {};
   QLineEdit* apiKeyEdit_ {};
};

class MapProviderPage::Impl
{
public:
   explicit Impl(MapProviderPage* self) : self_ {self} {};
   ~Impl() = default;

   void        SelectMapProvider(const QString& text);
   void        SelectMapProvider(map::MapProvider mapProvider);
   void        SetupMapProviderGroup(MapProviderGroup& group, int row);
   void        SetupSettingsInterface();
   static void SetGroupVisible(MapProviderGroup& group, bool visible);

   MapProviderPage* self_;

   QLayout* layout_ {};

   QFrame*      mapProviderFrame_ {};
   QGridLayout* mapProviderLayout_ {};
   QLabel*      mapProviderLabel_ {};
   QComboBox*   mapProviderComboBox_ {};
   QLabel*      descriptionLabel_ {};
   QFrame*      buttonFrame_ {};
   QHBoxLayout* buttonLayout_ {};
   QPushButton* apiKeyButton_ {};
   QSpacerItem* buttonSpacer_ {};

   MapProviderGroup mapboxGroup_ {};
   MapProviderGroup maptilerGroup_ {};

   map::MapProvider currentMapProvider_ {};

   settings::SettingsInterface<std::string> mapProvider_ {};
   settings::SettingsInterface<std::string> mapboxApiKey_ {};
   settings::SettingsInterface<std::string> mapTilerApiKey_ {};

   std::unordered_map<map::MapProvider, MapProviderGroup&> providerGroup_ {
      {map::MapProvider::Mapbox, mapboxGroup_},
      {map::MapProvider::MapTiler, maptilerGroup_}};
};

MapProviderPage::MapProviderPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>(this)}
{
   setTitle(tr("Map Provider"));
   setSubTitle(tr("Configure the Supercell Wx map provider."));

   p->mapProviderFrame_    = new QFrame(this);
   p->mapProviderLayout_   = new QGridLayout(p->mapProviderFrame_);
   p->mapProviderLabel_    = new QLabel(this);
   p->mapProviderComboBox_ = new QComboBox(this);
   p->descriptionLabel_    = new QLabel(this);
   p->buttonFrame_         = new QFrame(this);
   p->buttonLayout_        = new QHBoxLayout(p->buttonFrame_);
   p->apiKeyButton_        = new QPushButton(this);
   p->buttonSpacer_ =
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

   // Map Provider
   p->mapProviderLabel_->setText(tr("Map Provider"));

   for (const auto& mapProvider : map::MapProviderIterator())
   {
      p->mapProviderComboBox_->addItem(
         QString::fromStdString(map::GetMapProviderName(mapProvider)));
   }

   p->mapProviderLayout_->setContentsMargins(0, 0, 0, 0);
   p->mapProviderLayout_->addWidget(p->mapProviderLabel_, 0, 0, 1, 1);
   p->mapProviderLayout_->addWidget(p->mapProviderComboBox_, 0, 1, 1, 1);
   p->mapProviderFrame_->setLayout(p->mapProviderLayout_);

   // Description
   p->descriptionLabel_->setText(
      tr("You must get an API key from the map provider. After creating an "
         "account and reviewing terms of service, create an API key (or public "
         "token) with default scopes (unless one is created for you). Enter "
         "this API key here."));
   p->descriptionLabel_->setWordWrap(true);

   // API Key Button
   p->buttonLayout_->setContentsMargins(0, 0, 0, 0);
   p->buttonLayout_->addWidget(p->apiKeyButton_);
   p->buttonLayout_->addItem(p->buttonSpacer_);
   p->buttonFrame_->setLayout(p->buttonLayout_);

   p->SetupMapProviderGroup(p->mapboxGroup_, 1);
   p->SetupMapProviderGroup(p->maptilerGroup_, 2);

   // Overall layout
   p->layout_ = new QVBoxLayout(this);
   p->layout_->addWidget(p->mapProviderFrame_);
   p->layout_->addWidget(p->descriptionLabel_);
   p->layout_->addWidget(p->buttonFrame_);
   setLayout(p->layout_);

   // Configure settings interface
   p->SetupSettingsInterface();

   // Connect signals
   connect(p->mapProviderComboBox_,
           &QComboBox::currentTextChanged,
           this,
           [this](const QString& text) { p->SelectMapProvider(text); });

   connect(p->apiKeyButton_,
           &QAbstractButton::clicked,
           this,
           [this]()
           { QDesktopServices::openUrl(kUrl_.at(p->currentMapProvider_)); });

   // Map provider value
   map::MapProvider currentMapProvider = map::GetMapProvider(
      settings::GeneralSettings::Instance().map_provider().GetValue());
   std::string currentMapProviderName =
      map::GetMapProviderName(currentMapProvider);
   p->mapProviderComboBox_->setCurrentText(
      QString::fromStdString(currentMapProviderName));
   p->SelectMapProvider(currentMapProvider);
}

MapProviderPage::~MapProviderPage() = default;

void MapProviderPage::Impl::SetupMapProviderGroup(MapProviderGroup& group,
                                                  int               row)
{
   group.apiKeyLabel_ = new QLabel(self_);
   group.apiKeyEdit_  = new QLineEdit(self_);

   group.apiKeyLabel_->setText(tr("API Key"));

   mapProviderLayout_->addWidget(group.apiKeyLabel_, row, 0, 1, 1);
   mapProviderLayout_->addWidget(group.apiKeyEdit_, row, 1, 1, 1);

   QObject::connect(group.apiKeyEdit_,
                    &QLineEdit::textChanged,
                    self_,
                    &QWizardPage::completeChanged);
}

void MapProviderPage::Impl::SetupSettingsInterface()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   mapProvider_.SetSettingsVariable(generalSettings.map_provider());
   mapProvider_.SetMapFromValueFunction(
      [](const std::string& text) -> std::string
      {
         for (map::MapProvider mapProvider : map::MapProviderIterator())
         {
            const std::string mapProviderName =
               map::GetMapProviderName(mapProvider);

            if (boost::iequals(text, mapProviderName))
            {
               // Return map provider label
               return mapProviderName;
            }
         }

         // Map provider label not found, return unknown
         return "?";
      });
   mapProvider_.SetMapToValueFunction(
      [](std::string text) -> std::string
      {
         // Convert label to lower case and return
         boost::to_lower(text);
         return text;
      });
   mapProvider_.SetEditWidget(mapProviderComboBox_);

   mapboxApiKey_.SetSettingsVariable(generalSettings.mapbox_api_key());
   mapboxApiKey_.SetEditWidget(mapboxGroup_.apiKeyEdit_);

   mapTilerApiKey_.SetSettingsVariable(generalSettings.maptiler_api_key());
   mapTilerApiKey_.SetEditWidget(maptilerGroup_.apiKeyEdit_);
}

void MapProviderPage::Impl::SelectMapProvider(const QString& text)
{
   SelectMapProvider(map::GetMapProvider(text.toStdString()));
}

void MapProviderPage::Impl::SelectMapProvider(map::MapProvider mapProvider)
{
   std::string name = map::GetMapProviderName(mapProvider);

   switch (mapProvider)
   {
   case map::MapProvider::Mapbox:
      SetGroupVisible(mapboxGroup_, true);
      SetGroupVisible(maptilerGroup_, false);
      break;

   case map::MapProvider::MapTiler:
      SetGroupVisible(mapboxGroup_, false);
      SetGroupVisible(maptilerGroup_, true);
      break;

   default:
      break;
   }

   apiKeyButton_->setText(
      tr("Get %1 API Key").arg(QString::fromStdString(name)));

   currentMapProvider_ = mapProvider;

   Q_EMIT self_->completeChanged();
}

void MapProviderPage::Impl::SetGroupVisible(MapProviderGroup& group,
                                            bool              visible)
{
   group.apiKeyLabel_->setVisible(visible);
   group.apiKeyEdit_->setVisible(visible);
}

bool MapProviderPage::isComplete() const
{
   return p->providerGroup_.at(p->currentMapProvider_)
             .apiKeyEdit_->text()
             .size() > 1;
}

bool MapProviderPage::validatePage()
{
   bool committed = false;

   committed |= p->mapProvider_.Commit();
   committed |= p->mapboxApiKey_.Commit();
   committed |= p->mapTilerApiKey_.Commit();

   if (committed)
   {
      manager::SettingsManager::Instance().SaveSettings();
   }

   return true;
}

bool MapProviderPage::IsRequired()
{
   auto& generalSettings = settings::GeneralSettings::Instance();

   std::string mapboxApiKey   = generalSettings.mapbox_api_key().GetValue();
   std::string maptilerApiKey = generalSettings.maptiler_api_key().GetValue();

   // Setup is required if either API key is empty, or contains a single
   // character ("?")
   return (mapboxApiKey.size() <= 1 && maptilerApiKey.size() <= 1);
}

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
