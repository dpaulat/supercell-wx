#include <scwx/qt/settings/product_settings.hpp>
#include <scwx/qt/settings/settings_container.hpp>
#include <scwx/common/products.hpp>
#include <scwx/util/logger.hpp>

#include <cmath>
#include <functional>
#include <map>
#include <string>

namespace scwx::qt::settings
{

static const std::string logPrefix_ = "scwx::qt::settings::product_settings";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kThresholdsName_ {"thresholds"};
static const std::string kEnabledName_ {"enabled"};
static const std::string kValueName_ {"value"};

class ProductSettings::Impl
{
public:
   struct ThresholdData
   {
      ThresholdData()
      {
         enabled_.SetDefault(false);
         value_.SetDefault(0.0);
         value_.SetValidator([](double value) { return std::isfinite(value); });
      }

      SettingsVariable<bool>   enabled_ {kEnabledName_};
      SettingsVariable<double> value_ {kValueName_};

      friend bool operator==(const ThresholdData& lhs, const ThresholdData& rhs)
      {
         return lhs.enabled_ == rhs.enabled_ && lhs.value_ == rhs.value_;
      }
   };

   explicit Impl()
   {
      // SetDefault, SetMinimum and SetMaximum are descriptive
      // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
      showSmoothedRangeFolding_.SetDefault(false);
      stiForecastEnabled_.SetDefault(true);
      stiPastEnabled_.SetDefault(true);
      hailIndexEnabled_.SetDefault(true);
      mesocycloneEnabled_.SetDefault(true);
      tvsEnabled_.SetDefault(true);
      // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
   }

   ~Impl()                       = default;
   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   static std::string NormalizeProductName(common::RadarProductGroup group,
                                           const std::string& productName)
   {
      if (group == common::RadarProductGroup::Level3)
      {
         const std::string normalizedProduct =
            common::GetLevel3ProductByAwipsId(productName);

         if (normalizedProduct != "?")
         {
            return normalizedProduct;
         }
      }

      return productName;
   }

   static std::string ThresholdKey(common::RadarProductGroup group,
                                   const std::string&        productName)
   {
      if (group == common::RadarProductGroup::Unknown || productName.empty())
      {
         return {};
      }

      return common::GetRadarProductGroupName(group) + ":" +
             NormalizeProductName(group, productName);
   }

   ThresholdData& GetOrCreateThresholdData(const std::string& key)
   {
      return thresholdData_.try_emplace(key).first->second;
   }

   std::optional<std::reference_wrapper<ThresholdData>>
   FindThresholdData(const std::string& key)
   {
      const auto it = thresholdData_.find(key);

      if (it == thresholdData_.end())
      {
         return std::nullopt;
      }

      return std::ref(it->second);
   }

   [[nodiscard]] std::optional<std::reference_wrapper<const ThresholdData>>
   FindThresholdData(const std::string& key) const
   {
      const auto it = thresholdData_.find(key);

      if (it == thresholdData_.end())
      {
         return std::nullopt;
      }

      return std::cref(it->second);
   }

   SettingsVariable<bool> showSmoothedRangeFolding_ {
      "show_smoothed_range_folding"};
   SettingsVariable<bool> stiForecastEnabled_ {"sti_forecast_enabled"};
   SettingsVariable<bool> stiPastEnabled_ {"sti_past_enabled"};
   SettingsVariable<bool> hailIndexEnabled_ {"hail_index_enabled"};
   SettingsVariable<bool> mesocycloneEnabled_ {"mesocyclone_enabled"};
   SettingsVariable<bool> tvsEnabled_ {"tvs_enabled"};
   std::map<std::string, ThresholdData> thresholdData_ {};
};

ProductSettings::ProductSettings() :
    SettingsCategory("product"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->showSmoothedRangeFolding_,
                      &p->stiForecastEnabled_,
                      &p->stiPastEnabled_,
                      &p->hailIndexEnabled_,
                      &p->mesocycloneEnabled_,
                      &p->tvsEnabled_});
   SetDefaults();
}
ProductSettings::~ProductSettings() = default;

ProductSettings::ProductSettings(ProductSettings&&) noexcept = default;
ProductSettings&
ProductSettings::operator=(ProductSettings&&) noexcept = default;

SettingsVariable<bool>& ProductSettings::show_smoothed_range_folding()
{
   return p->showSmoothedRangeFolding_;
}

SettingsVariable<bool>& ProductSettings::sti_forecast_enabled()
{
   return p->stiForecastEnabled_;
}

SettingsVariable<bool>& ProductSettings::sti_past_enabled()
{
   return p->stiPastEnabled_;
}

SettingsVariable<bool>& ProductSettings::hail_index_enabled()
{
   return p->hailIndexEnabled_;
}

SettingsVariable<bool>& ProductSettings::mesocyclone_enabled()
{
   return p->mesocycloneEnabled_;
}

SettingsVariable<bool>& ProductSettings::tvs_enabled()
{
   return p->tvsEnabled_;
}

std::optional<float>
ProductSettings::color_table_threshold(common::RadarProductGroup group,
                                       const std::string& productName) const
{
   const std::string thresholdKey = Impl::ThresholdKey(group, productName);

   if (thresholdKey.empty())
   {
      return std::nullopt;
   }

   auto thresholdData = p->FindThresholdData(thresholdKey);

   if (!thresholdData.has_value() ||
       !thresholdData->get().enabled_.GetStagedOrValue())
   {
      return std::nullopt;
   }

   return static_cast<float>(thresholdData->get().value_.GetStagedOrValue());
}

void ProductSettings::set_color_table_threshold(common::RadarProductGroup group,
                                                const std::string& productName,
                                                std::optional<float> threshold)
{
   const std::string thresholdKey = p->ThresholdKey(group, productName);

   if (thresholdKey.empty())
   {
      return;
   }

   if (threshold.has_value())
   {
      auto& thresholdData = p->GetOrCreateThresholdData(thresholdKey);
      thresholdData.value_.StageValue(*threshold);
      thresholdData.enabled_.StageValue(true);
   }
   else
   {
      auto thresholdData = p->FindThresholdData(thresholdKey);

      if (thresholdData.has_value())
      {
         thresholdData->get().enabled_.StageValue(false);
      }
   }
}

bool ProductSettings::Shutdown()
{
   bool dataChanged = false;

   // Commit settings that are managed separate from the settings dialog
   dataChanged |= p->stiForecastEnabled_.Commit();
   dataChanged |= p->stiPastEnabled_.Commit();
   dataChanged |= p->hailIndexEnabled_.Commit();
   dataChanged |= p->mesocycloneEnabled_.Commit();
   dataChanged |= p->tvsEnabled_.Commit();

   for (auto& thresholdEntry : p->thresholdData_)
   {
      auto& thresholdData = thresholdEntry.second;

      dataChanged |= thresholdData.enabled_.Commit();
      dataChanged |= thresholdData.value_.Commit();
   }

   return dataChanged;
}

bool ProductSettings::ReadJson(const boost::json::object& json)
{
   bool validated = SettingsCategory::ReadJson(json);

   p->thresholdData_.clear();

   const boost::json::value* value = json.if_contains(name());

   if (value != nullptr && value->is_object())
   {
      const boost::json::object& object = value->as_object();
      const boost::json::value*  thresholdsValue =
         object.if_contains(kThresholdsName_);

      if (thresholdsValue != nullptr && thresholdsValue->is_object())
      {
         const boost::json::object& thresholdsObject =
            thresholdsValue->as_object();

         for (const auto& thresholdEntry : thresholdsObject)
         {
            if (!thresholdEntry.value().is_object())
            {
               logger_->warn(
                  "Invalid json for threshold key {}, ignoring entry",
                  thresholdEntry.key());
               validated = false;
               continue;
            }

            auto& thresholdData =
               p->GetOrCreateThresholdData(std::string(thresholdEntry.key()));
            const boost::json::object& thresholdObject =
               thresholdEntry.value().as_object();

            validated &= thresholdData.enabled_.ReadValue(thresholdObject);
            validated &= thresholdData.value_.ReadValue(thresholdObject);
         }
      }
      else
      {
         if (thresholdsValue == nullptr)
         {
            logger_->debug("Key {}.{} is not present, resetting to defaults",
                           name(),
                           kThresholdsName_);
         }
         else
         {
            logger_->warn("Invalid json for key {}.{}, resetting to defaults",
                          name(),
                          kThresholdsName_);
         }

         validated = false;
      }
   }

   return validated;
}

void ProductSettings::WriteJson(boost::json::object& json) const
{
   SettingsCategory::WriteJson(json);

   boost::json::object thresholdsObject;

   for (const auto& thresholdEntry : p->thresholdData_)
   {
      boost::json::object thresholdObject;

      thresholdEntry.second.enabled_.WriteValue(thresholdObject);
      thresholdEntry.second.value_.WriteValue(thresholdObject);

      thresholdsObject.insert_or_assign(thresholdEntry.first, thresholdObject);
   }

   boost::json::object& object = json.at(name()).as_object();
   object.insert_or_assign(kThresholdsName_, thresholdsObject);
}

ProductSettings& ProductSettings::Instance()
{
   static ProductSettings generalSettings_;
   return generalSettings_;
}

bool operator==(const ProductSettings& lhs, const ProductSettings& rhs)
{
   return (lhs.p->showSmoothedRangeFolding_ ==
              rhs.p->showSmoothedRangeFolding_ &&
           lhs.p->stiForecastEnabled_ == rhs.p->stiForecastEnabled_ &&
           lhs.p->stiPastEnabled_ == rhs.p->stiPastEnabled_ &&
           lhs.p->hailIndexEnabled_ == rhs.p->hailIndexEnabled_ &&
           lhs.p->mesocycloneEnabled_ == rhs.p->mesocycloneEnabled_ &&
           lhs.p->tvsEnabled_ == rhs.p->tvsEnabled_ &&
           lhs.p->thresholdData_ == rhs.p->thresholdData_);
}

} // namespace scwx::qt::settings
