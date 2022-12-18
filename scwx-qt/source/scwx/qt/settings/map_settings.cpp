#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/util/json.hpp>
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

static constexpr std::size_t kCount_            = 4u;
static const std::string     kDefaultRadarSite_ = "KLSX";

static const std::string kRadarSiteName_ {"radar_site"};
static const std::string kRadarProductGroupName_ {"radar_product_group"};
static const std::string kRadarProductName_ {"radar_product"};

static constexpr common::RadarProductGroup kDefaultRadarProductGroup_ =
   common::RadarProductGroup::Level3;
static const std::string kDefaultRadarProductGroupString_ = "L3";
static const std::array<std::string, kCount_> kDefaultRadarProduct_ {
   "N0B", "N0G", "N0C", "N0X"};

class MapSettingsImpl
{
public:
   struct MapData
   {
      SettingsVariable<std::string> radarSite_ {kRadarSiteName_};
      SettingsVariable<std::string> radarProductGroup_ {
         kRadarProductGroupName_};
      SettingsVariable<std::string> radarProduct_ {kRadarProductName_};
   };

   explicit MapSettingsImpl()
   {
      for (std::size_t i = 0; i < kCount_; i++)
      {
         map_[i].radarSite_.SetDefault(kDefaultRadarSite_);
         map_[i].radarProductGroup_.SetDefault(
            kDefaultRadarProductGroupString_);
         map_[i].radarProduct_.SetDefault(kDefaultRadarProduct_[i]);

         map_[i].radarSite_.SetValidator(
            [](const std::string& value)
            {
               // Radar site must exist
               return config::RadarSite::Get(value) != nullptr;
            });

         map_[i].radarProductGroup_.SetValidator(
            [](const std::string& value)
            {
               // Radar product group must be valid
               common::RadarProductGroup radarProductGroup =
                  common::GetRadarProductGroup(value);
               return radarProductGroup != common::RadarProductGroup::Unknown;
            });

         map_[i].radarProduct_.SetValidator(
            [this, i](const std::string& value)
            {
               common::RadarProductGroup radarProductGroup =
                  common::GetRadarProductGroup(
                     map_[i].radarProductGroup_.GetValue());

               if (radarProductGroup == common::RadarProductGroup::Level2)
               {
                  // Radar product must be valid
                  return common::GetLevel2Product(value) !=
                         common::Level2Product::Unknown;
               }
               else
               {
                  // TODO: Validate level 3 product
                  return true;
               }
            });

         variables_.insert(variables_.cend(),
                           {&map_[i].radarSite_,
                            &map_[i].radarProductGroup_,
                            &map_[i].radarProduct_});
      }
   }

   ~MapSettingsImpl() {}

   void SetDefaults(std::size_t i)
   {
      map_[i].radarSite_.SetValueToDefault();
      map_[i].radarProductGroup_.SetValueToDefault();
      map_[i].radarProduct_.SetValueToDefault();
   }

   std::array<MapData, kCount_>       map_ {};
   std::vector<SettingsVariableBase*> variables_ {};
};

MapSettings::MapSettings() :
    SettingsCategory("maps"), p(std::make_unique<MapSettingsImpl>())
{
   RegisterVariables(p->variables_);
   SetDefaults();

   p->variables_.clear();
}
MapSettings::~MapSettings() = default;

MapSettings::MapSettings(MapSettings&&) noexcept            = default;
MapSettings& MapSettings::operator=(MapSettings&&) noexcept = default;

std::size_t MapSettings::count() const
{
   return kCount_;
}

std::string MapSettings::radar_site(std::size_t i) const
{
   return p->map_[i].radarSite_.GetValue();
}

common::RadarProductGroup MapSettings::radar_product_group(std::size_t i) const
{
   return common::GetRadarProductGroup(
      p->map_[i].radarProductGroup_.GetValue());
}

std::string MapSettings::radar_product(std::size_t i) const
{
   return p->map_[i].radarProduct_.GetValue();
}

bool MapSettings::ReadJson(const boost::json::object& json)
{
   bool validated = true;

   const boost::json::value* value = json.if_contains(name());

   if (value != nullptr && value->is_array())
   {
      const boost::json::array& mapArray = value->as_array();

      for (std::size_t i = 0; i < kCount_; ++i)
      {
         if (i < mapArray.size() && mapArray.at(i).is_object())
         {
            const boost::json::object& mapRecord = mapArray.at(i).as_object();
            MapSettingsImpl::MapData&  mapRecordSettings = p->map_[i];

            // Load JSON Elements
            validated &= mapRecordSettings.radarSite_.ReadValue(mapRecord);
            validated &=
               mapRecordSettings.radarProductGroup_.ReadValue(mapRecord);

            bool productValidated =
               mapRecordSettings.radarProduct_.ReadValue(mapRecord);
            if (!productValidated)
            {
               // Product was set to default, reset group to default to match
               mapRecordSettings.radarProductGroup_.SetValueToDefault();
               validated = false;
            }
         }
         else
         {
            logger_->warn(
               "Too few array entries, resetting record {} to defaults", i + 1);
            validated = false;
            p->SetDefaults(i);
         }
      }
   }
   else
   {
      if (value == nullptr)
      {
         logger_->warn("Key is not present, resetting to defaults");
      }
      else if (!value->is_array())
      {
         logger_->warn("Invalid json, resetting to defaults");
      }

      SetDefaults();
      validated = false;
   }

   return validated;
}

void MapSettings::WriteJson(boost::json::object& json) const
{
   boost::json::value object = boost::json::value_from(p->map_);
   json.insert_or_assign(name(), object);
}

void tag_invoke(boost::json::value_from_tag,
                boost::json::value&             jv,
                const MapSettingsImpl::MapData& data)
{
   jv = {{kRadarSiteName_, data.radarSite_.GetValue()},
         {kRadarProductGroupName_, data.radarProductGroup_.GetValue()},
         {kRadarProductName_, data.radarProduct_.GetValue()}};
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
