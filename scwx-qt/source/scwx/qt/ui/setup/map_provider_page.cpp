#include <scwx/qt/ui/setup/map_provider_page.hpp>
#include <scwx/qt/map/map_provider.hpp>
#include <scwx/qt/settings/general_settings.hpp>

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

namespace scwx
{
namespace qt
{
namespace ui
{
namespace setup
{

struct MapProviderGroup
{
   QLabel*    apiKeyLabel_ {};
   QLineEdit* apiKeyEdit_ {};
};

class MapProviderPage::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   void        SelectMapProvider(const QString& text);
   void        SelectMapProvider(map::MapProvider mapProvider);
   void        SetupMapProviderGroup(MapProviderPage*  parent,
                                     MapProviderGroup& group,
                                     int               row);
   static void SetGroupVisible(MapProviderGroup& group, bool visible);

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
};

MapProviderPage::MapProviderPage(QWidget* parent) :
    QWizardPage(parent), p {std::make_shared<Impl>()}
{
   setTitle(tr("Map Provider"));
   setSubTitle(tr("Configure the Supercell Wx map provider."));

   p->mapProviderFrame_    = new QFrame(parent);
   p->mapProviderLayout_   = new QGridLayout(parent);
   p->mapProviderLabel_    = new QLabel(parent);
   p->mapProviderComboBox_ = new QComboBox(parent);
   p->descriptionLabel_    = new QLabel(parent);
   p->buttonFrame_         = new QFrame(parent);
   p->buttonLayout_        = new QHBoxLayout(parent);
   p->apiKeyButton_        = new QPushButton(parent);
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
      ("You must get an API key from the map provider. After creating an "
       "account and reviewing terms of service, create an API key (or public "
       "token) with default scopes (unless one is created for you). Enter "
       "this API key here."));
   p->descriptionLabel_->setWordWrap(true);

   // API Key Button
   p->buttonLayout_->setContentsMargins(0, 0, 0, 0);
   p->buttonLayout_->addWidget(p->apiKeyButton_);
   p->buttonLayout_->addItem(p->buttonSpacer_);
   p->buttonFrame_->setLayout(p->buttonLayout_);

   p->SetupMapProviderGroup(this, p->mapboxGroup_, 1);
   p->SetupMapProviderGroup(this, p->maptilerGroup_, 2);

   connect(p->mapProviderComboBox_,
           &QComboBox::currentTextChanged,
           this,
           [this](const QString& text) { p->SelectMapProvider(text); });

   connect(p->apiKeyButton_,
           &QAbstractButton::clicked,
           this,
           [this]()
           {
              switch (p->currentMapProvider_)
              {
              case map::MapProvider::Mapbox:
                 QDesktopServices::openUrl(QUrl {"https://www.mapbox.com/"});
                 break;

              case map::MapProvider::MapTiler:
                 QDesktopServices::openUrl(QUrl {"https://www.maptiler.com/"});
                 break;

              default:
                 break;
              }
           });

   // Map provider value
   map::MapProvider defaultMapProvider = map::GetMapProvider(
      settings::GeneralSettings::Instance().map_provider().GetDefault());
   std::string defaultMapProviderName =
      map::GetMapProviderName(defaultMapProvider);
   p->mapProviderComboBox_->setCurrentText(
      QString::fromStdString(defaultMapProviderName));
   p->SelectMapProvider(defaultMapProvider);

   p->layout_ = new QVBoxLayout(this);
   p->layout_->addWidget(p->mapProviderFrame_);
   p->layout_->addWidget(p->descriptionLabel_);
   p->layout_->addWidget(p->buttonFrame_);
   setLayout(p->layout_);
}

MapProviderPage::~MapProviderPage() = default;

void MapProviderPage::Impl::SetupMapProviderGroup(MapProviderPage*  parent,
                                                  MapProviderGroup& group,
                                                  int               row)
{
   group.apiKeyLabel_ = new QLabel(parent);
   group.apiKeyEdit_  = new QLineEdit(parent);

   group.apiKeyLabel_->setText("API Key");

   mapProviderLayout_->addWidget(group.apiKeyLabel_, row, 0, 1, 1);
   mapProviderLayout_->addWidget(group.apiKeyEdit_, row, 1, 1, 1);
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
}

void MapProviderPage::Impl::SetGroupVisible(MapProviderGroup& group,
                                            bool              visible)
{
   group.apiKeyLabel_->setVisible(visible);
   group.apiKeyEdit_->setVisible(visible);
}

} // namespace setup
} // namespace ui
} // namespace qt
} // namespace scwx
