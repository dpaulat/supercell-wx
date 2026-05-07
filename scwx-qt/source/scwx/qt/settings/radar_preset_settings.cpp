#include <scwx/qt/settings/radar_preset_settings.hpp>
#include <scwx/qt/settings/settings_variable.hpp>
#include <scwx/util/logger.hpp>

#include <boost/json.hpp>

namespace scwx::qt::settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::radar_preset_settings";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static const std::string kActivePresetName_ {"active_preset"};
static const std::string kPresetsName_ {"presets"};
static const std::string kGroupKey_ {"radar_product_group"};
static const std::string kProductKey_ {"radar_product"};

static const std::string kDefaultGroup_ = "L3";
static const std::array<std::string, RadarPresetSettings::kMapCount_>
   kDefaultProducts_ {
      "N0B", "N0G", "N0C", "N0X", "DVL", "EET", "N0S", "N0H", "N0K"};

class RadarPresetSettings::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void SetDefaults()
   {
      presets_.clear();
      activePreset_.clear();
   }

   std::vector<Preset> presets_ {};
   std::string         activePreset_ {};
};

RadarPresetSettings::RadarPresetSettings() :
    SettingsCategory("radar_presets"), p(std::make_unique<Impl>())
{
}
RadarPresetSettings::~RadarPresetSettings() = default;

RadarPresetSettings::RadarPresetSettings(RadarPresetSettings&&) noexcept =
   default;
RadarPresetSettings&
RadarPresetSettings::operator=(RadarPresetSettings&&) noexcept = default;

std::vector<RadarPresetSettings::Preset>& RadarPresetSettings::presets()
{
   return p->presets_;
}

const std::vector<RadarPresetSettings::Preset>&
RadarPresetSettings::presets() const
{
   return p->presets_;
}

std::string RadarPresetSettings::active_preset() const
{
   return p->activePreset_;
}

void RadarPresetSettings::set_active_preset(const std::string& name)
{
   p->activePreset_ = name;
}

std::optional<std::size_t>
RadarPresetSettings::FindPresetIndex(const std::string& name) const
{
   for (std::size_t i = 0; i < p->presets_.size(); ++i)
   {
      if (p->presets_[i].name == name)
      {
         return i;
      }
   }
   return std::nullopt;
}

bool RadarPresetSettings::ReadJson(const boost::json::object& json)
{
   bool validated = true;

   p->SetDefaults();

   const boost::json::value* value = json.if_contains(name());
   if (value == nullptr || !value->is_object())
   {
      if (value == nullptr)
      {
         logger_->debug("Key \"{}\" is not present, using empty presets",
                        name());
      }
      else
      {
         logger_->warn("Invalid json for \"{}\", using empty presets", name());
         validated = false;
      }
      return validated;
   }

   const boost::json::object& obj = value->as_object();

   // Read active_preset
   const boost::json::value* apValue = obj.if_contains(kActivePresetName_);
   if (apValue != nullptr && apValue->is_string())
   {
      p->activePreset_ = apValue->as_string().c_str();
   }

   // Read presets array
   const boost::json::value* presetsValue = obj.if_contains(kPresetsName_);
   if (presetsValue != nullptr && presetsValue->is_array())
   {
      const boost::json::array& arr = presetsValue->as_array();
      p->presets_.clear();

      for (const auto& element : arr)
      {
         if (!element.is_object())
         {
            logger_->warn("Invalid preset entry in array, skipping");
            validated = false;
            continue;
         }

         const boost::json::object& presetObj = element.as_object();
         Preset                     preset;

         // Read name
         const boost::json::value* nameVal = presetObj.if_contains("name");
         if (nameVal != nullptr && nameVal->is_string())
         {
            preset.name = nameVal->as_string().c_str();
         }
         else
         {
            logger_->warn("Preset missing name, skipping");
            validated = false;
            continue;
         }

         // Read grid_width
         const boost::json::value* gwVal = presetObj.if_contains("grid_width");
         if (gwVal != nullptr && gwVal->is_int64())
         {
            preset.gridWidth = gwVal->as_int64();
         }
         else
         {
            logger_->warn("Preset \"{}\" missing grid_width, defaulting to 1",
                          preset.name);
            preset.gridWidth = 1;
            validated        = false;
         }

         // Read grid_height
         const boost::json::value* ghVal = presetObj.if_contains("grid_height");
         if (ghVal != nullptr && ghVal->is_int64())
         {
            preset.gridHeight = ghVal->as_int64();
         }
         else
         {
            logger_->warn("Preset \"{}\" missing grid_height, defaulting to 1",
                          preset.name);
            preset.gridHeight = 1;
            validated         = false;
         }

         // Read row_sizes (optional, backward compatible)
         const boost::json::value* rsVal = presetObj.if_contains("row_sizes");
         if (rsVal != nullptr && rsVal->is_array())
         {
            preset.rowSizes.clear();
            for (const auto& v : rsVal->as_array())
               if (v.is_int64())
                  preset.rowSizes.push_back(static_cast<int>(v.as_int64()));
         }

         // Read column_sizes (optional, backward compatible)
         const boost::json::value* csVal =
            presetObj.if_contains("column_sizes");
         if (csVal != nullptr && csVal->is_array())
         {
            preset.columnSizes.clear();
            for (const auto& v : csVal->as_array())
               if (v.is_int64())
                  preset.columnSizes.push_back(static_cast<int>(v.as_int64()));
         }

         // Read products array
         const boost::json::value* prodVal = presetObj.if_contains("products");
         if (prodVal != nullptr && prodVal->is_array())
         {
            const boost::json::array& prodArr = prodVal->as_array();

            for (std::size_t i = 0; i < kMapCount_; ++i)
            {
               if (i < prodArr.size() && prodArr[i].is_object())
               {
                  const boost::json::object& prodObj = prodArr[i].as_object();

                  const boost::json::value* gVal =
                     prodObj.if_contains(kGroupKey_);
                  if (gVal != nullptr && gVal->is_string())
                  {
                     preset.products[i].group = gVal->as_string().c_str();
                  }
                  else
                  {
                     preset.products[i].group = kDefaultGroup_;
                     validated                = false;
                  }

                  const boost::json::value* pVal =
                     prodObj.if_contains(kProductKey_);
                  if (pVal != nullptr && pVal->is_string())
                  {
                     preset.products[i].name = pVal->as_string().c_str();
                  }
                  else
                  {
                     preset.products[i].name = kDefaultProducts_[i];
                     validated               = false;
                  }
               }
               else
               {
                  // Missing product entry, fill with defaults
                  preset.products[i].group = kDefaultGroup_;
                  preset.products[i].name  = kDefaultProducts_[i];
                  validated                = false;
               }
            }
         }
         else
         {
            logger_->warn("Preset \"{}\" missing products, defaulting all",
                          preset.name);
            for (std::size_t i = 0; i < kMapCount_; ++i)
            {
               preset.products[i].group = kDefaultGroup_;
               preset.products[i].name  = kDefaultProducts_[i];
            }
            validated = false;
         }

         p->presets_.push_back(std::move(preset));
      }
   }
   else
   {
      if (presetsValue == nullptr)
      {
         logger_->debug("No presets found in \"{}\", starting empty", name());
      }
      else
      {
         logger_->warn("Invalid presets value in \"{}\"", name());
         validated = false;
      }
   }

   return validated;
}

void RadarPresetSettings::WriteJson(boost::json::object& json) const
{
   boost::json::object obj;
   obj[kActivePresetName_] = p->activePreset_;
   obj[kPresetsName_]      = boost::json::value_from(p->presets_);
   json.insert_or_assign(name(), std::move(obj));
}

RadarPresetSettings& RadarPresetSettings::Instance()
{
   static RadarPresetSettings radarPresetSettings_;
   return radarPresetSettings_;
}

void tag_invoke(boost::json::value_from_tag,
                boost::json::value&                              jv,
                const RadarPresetSettings::Preset::ProductEntry& product)
{
   jv = {{kGroupKey_, product.group}, {kProductKey_, product.name}};
}

void tag_invoke(boost::json::value_from_tag,
                boost::json::value&                jv,
                const RadarPresetSettings::Preset& preset)
{
   jv = {{"name", preset.name},
         {"grid_width", preset.gridWidth},
         {"grid_height", preset.gridHeight},
         {"row_sizes", boost::json::value_from(preset.rowSizes)},
         {"column_sizes", boost::json::value_from(preset.columnSizes)},
         {"products", boost::json::value_from(preset.products)}};
}

} // namespace scwx::qt::settings
