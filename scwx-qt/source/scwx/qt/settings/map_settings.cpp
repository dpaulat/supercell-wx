#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/common/products.hpp>
#include <scwx/util/logger.hpp>

#include <array>
#include <execution>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::map_settings";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t  kCount_                    = 4u;
static const std::string kDefaultRadarSite_         = "KLSX";
static const std::string kDefaultRadarProductGroup_ = "L3";

static const std::array<std::string, kCount_> kDefaultRadarProduct_ {
   "N0B", "N0G", "N0C", "N0X"};

class MapSettingsImpl
{
public:
   struct MapData
   {
      std::string radarSite_;
      std::string radarProductGroup_;
      std::string radarProduct_;
   };

   explicit MapSettingsImpl() { SetDefaults(); }

   ~MapSettingsImpl() {}

   void SetDefaults(size_t i)
   {
      map_[i].radarSite_         = kDefaultRadarSite_;
      map_[i].radarProductGroup_ = kDefaultRadarProductGroup_;
      map_[i].radarProduct_      = kDefaultRadarProduct_[i];
   }

   void SetDefaults()
   {
      for (size_t i = 0; i < kCount_; i++)
      {
         SetDefaults(i);
      }
   }

   std::array<MapData, kCount_> map_;
};

MapSettings::MapSettings() : p(std::make_unique<MapSettingsImpl>()) {}
MapSettings::~MapSettings() = default;

MapSettings::MapSettings(MapSettings&&) noexcept = default;
MapSettings& MapSettings::operator=(MapSettings&&) noexcept = default;

size_t MapSettings::count() const
{
   return kCount_;
}

std::string MapSettings::radar_site(size_t i) const
{
   return p->map_[i].radarSite_;
}

std::string MapSettings::radar_product_group(size_t i) const
{
   return p->map_[i].radarProductGroup_;
}

std::string MapSettings::radar_product(size_t i) const
{
   return p->map_[i].radarProduct_;
}

boost::json::value MapSettings::ToJson() const
{
   boost::json::value json;

   json = boost::json::value_from(p->map_);

   return json;
}

std::shared_ptr<MapSettings> MapSettings::Create()
{
   std::shared_ptr<MapSettings> generalSettings =
      std::make_shared<MapSettings>();

   generalSettings->p->SetDefaults();

   return generalSettings;
}

std::shared_ptr<MapSettings> MapSettings::Load(const boost::json::value* json,
                                               bool& jsonDirty)
{
   std::shared_ptr<MapSettings> mapSettings = std::make_shared<MapSettings>();

   if (json != nullptr && json->is_array())
   {
      const boost::json::array& mapArray = json->as_array();

      for (size_t i = 0; i < kCount_; ++i)
      {
         if (i < mapArray.size() && mapArray.at(i).is_object())
         {
            const boost::json::object& mapRecord = mapArray.at(i).as_object();
            MapSettingsImpl::MapData&  mapRecordSettings =
               mapSettings->p->map_[i];

            // Load JSON Elements
            jsonDirty |=
               !util::json::FromJsonString(mapRecord,
                                           "radar_site",
                                           mapRecordSettings.radarSite_,
                                           kDefaultRadarSite_);
            jsonDirty |=
               !util::json::FromJsonString(mapRecord,
                                           "radar_product_group",
                                           mapRecordSettings.radarProductGroup_,
                                           kDefaultRadarSite_);
            jsonDirty |=
               !util::json::FromJsonString(mapRecord,
                                           "radar_product",
                                           mapRecordSettings.radarProduct_,
                                           kDefaultRadarSite_);

            // Validate Radar Site
            if (config::RadarSite::Get(mapRecordSettings.radarSite_) == nullptr)
            {
               mapRecordSettings.radarSite_ = kDefaultRadarSite_;
               jsonDirty                    = true;
            }

            // Validate Radar Product Group
            common::RadarProductGroup radarProductGroup =
               common::GetRadarProductGroup(
                  mapRecordSettings.radarProductGroup_);
            if (radarProductGroup == common::RadarProductGroup::Unknown)
            {
               mapRecordSettings.radarProductGroup_ =
                  kDefaultRadarProductGroup_;
               radarProductGroup =
                  common::GetRadarProductGroup(kDefaultRadarProductGroup_);
               jsonDirty = true;
            }

            // Validate Radar Product
            if (radarProductGroup == common::RadarProductGroup::Level2 &&
                common::GetLevel2Product(mapRecordSettings.radarProduct_) ==
                   common::Level2Product::Unknown)
            {
               mapRecordSettings.radarProductGroup_ =
                  kDefaultRadarProductGroup_;
               mapRecordSettings.radarProduct_ = kDefaultRadarProduct_[i];
               jsonDirty                       = true;
            }

            // TODO: Validate level 3 product
         }
         else
         {
            logger_->warn(
               "Too few array entries, resetting record {} to defaults", i + 1);
            jsonDirty = true;
            mapSettings->p->SetDefaults(i);
         }
      }
   }
   else
   {
      if (json == nullptr)
      {
         logger_->warn("Key is not present, resetting to defaults");
      }
      else if (!json->is_array())
      {
         logger_->warn("Invalid json, resetting to defaults");
      }

      mapSettings->p->SetDefaults();
      jsonDirty = true;
   }

   return mapSettings;
}

void tag_invoke(boost::json::value_from_tag,
                boost::json::value&             jv,
                const MapSettingsImpl::MapData& data)
{
   jv = {{"radar_site", data.radarSite_},
         {"radar_product_group", data.radarProductGroup_},
         {"radar_product", data.radarProduct_}};
}

bool operator==(const MapSettings& lhs, const MapSettings& rhs)
{
   return (lhs.p->map_ == rhs.p->map_);
}

bool operator==(const MapSettingsImpl::MapData& lhs,
                const MapSettingsImpl::MapData& rhs)
{
   return (lhs.radarSite_ == rhs.radarSite_ &&
           lhs.radarProductGroup_ == rhs.radarProductGroup_ &&
           lhs.radarProduct_ == rhs.radarProduct_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
