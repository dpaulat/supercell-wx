#include <scwx/qt/settings/map_settings.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/common/products.hpp>
#include <scwx/util/logger.hpp>

#include <array>
#include <execution>

#include <boost/json.hpp>

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

static const std::string kMapStyleName_ {"map_style"};
static const std::string kRadarSiteName_ {"radar_site"};
static const std::string kRadarProductGroupName_ {"radar_product_group"};
static const std::string kRadarProductName_ {"radar_product"};

static const std::string                   kDefaultMapStyle_ {"?"};
//static constexpr common::RadarProductGroup kDefaultRadarProductGroup_ =
//   common::RadarProductGroup::Level3;
static const std::string kDefaultRadarProductGroupString_ = "L3";
static const std::array<std::string, kCount_> kDefaultRadarProduct_ {
   "N0B", "N0G", "N0C", "N0X"};

class MapSettings::Impl
{
public:
   struct MapData
   {
      SettingsVariable<std::string> mapStyle_ {kMapStyleName_};
      SettingsVariable<std::string> radarSite_ {kRadarSiteName_};
      SettingsVariable<std::string> radarProductGroup_ {
         kRadarProductGroupName_};
      SettingsVariable<std::string> radarProduct_ {kRadarProductName_};
   };

   explicit Impl()
   {
      for (std::size_t i = 0; i < kCount_; i++)
      {
         map_[i].mapStyle_.SetDefault(kDefaultMapStyle_);
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
                           {&map_[i].mapStyle_,
                            &map_[i].radarSite_,
                            &map_[i].radarProductGroup_,
                            &map_[i].radarProduct_});
      }
   }

   ~Impl() {}

   void SetDefaults(std::size_t i)
   {
      map_[i].mapStyle_.SetValueToDefault();
      map_[i].radarSite_.SetValueToDefault();
      map_[i].radarProductGroup_.SetValueToDefault();
      map_[i].radarProduct_.SetValueToDefault();
   }

   friend void tag_invoke(boost::json::value_from_tag,
                          boost::json::value& jv,
                          const MapData&      data)
   {
      jv = {{kMapStyleName_, data.mapStyle_.GetValue()},
            {kRadarSiteName_, data.radarSite_.GetValue()},
            {kRadarProductGroupName_, data.radarProductGroup_.GetValue()},
            {kRadarProductName_, data.radarProduct_.GetValue()}};
   }

   friend bool operator==(const MapData& lhs, const MapData& rhs)
   {
      return (lhs.mapStyle_ == rhs.mapStyle_ && //
              lhs.radarSite_ == rhs.radarSite_ &&
              lhs.radarProductGroup_ == rhs.radarProductGroup_ &&
              lhs.radarProduct_ == rhs.radarProduct_);
   }

   std::array<MapData, kCount_>       map_ {};
   std::vector<SettingsVariableBase*> variables_ {};
};

MapSettings::MapSettings() :
    SettingsCategory("maps"), p(std::make_unique<Impl>())
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

SettingsVariable<std::string>& MapSettings::map_style(std::size_t i) const
{
   return p->map_[i].mapStyle_;
}

SettingsVariable<std::string>& MapSettings::radar_site(std::size_t i) const
{
   return p->map_[i].radarSite_;
}

SettingsVariable<std::string>&
MapSettings::radar_product_group(std::size_t i) const
{
   return p->map_[i].radarProductGroup_;
}

SettingsVariable<std::string>& MapSettings::radar_product(std::size_t i) const
{
   return p->map_[i].radarProduct_;
}

bool MapSettings::Shutdown()
{
   bool dataChanged = false;

   // Commit settings that are managed separate from the settings dialog
   for (std::size_t i = 0; i < kCount_; ++i)
   {
      Impl::MapData& mapRecordSettings = p->map_[i];

      dataChanged |= mapRecordSettings.mapStyle_.Commit();
   }

   return dataChanged;
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
            Impl::MapData&             mapRecordSettings = p->map_[i];

            // Load JSON Elements
            validated &= mapRecordSettings.mapStyle_.ReadValue(mapRecord);
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

MapSettings& MapSettings::Instance()
{
   static MapSettings mapSettings_;
   return mapSettings_;
}

bool operator==(const MapSettings& lhs, const MapSettings& rhs)
{
   return (lhs.p->map_ == rhs.p->map_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
