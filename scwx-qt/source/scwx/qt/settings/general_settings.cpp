#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "scwx::qt::settings::general_settings";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr bool             kDefaultDebugEnabled_ {false};
static const std::string          kDefaultDefaultRadarSite_ {"KLSX"};
static const std::vector<int64_t> kDefaultFontSizes_ {16};
static constexpr int64_t          kDefaultGridWidth_ {1};
static constexpr int64_t          kDefaultGridHeight_ {1};
static const std::string          kDefaultMapboxApiKey_ {"?"};

static constexpr int64_t kFontSizeMinimum_ {1};
static constexpr int64_t kFontSizeMaximum_ {72};
static constexpr int64_t kGridWidthMinimum_ {1};
static constexpr int64_t kGridWidthMaximum_ {2};
static constexpr int64_t kGridHeightMinimum_ {1};
static constexpr int64_t kGridHeightMaximum_ {2};

class GeneralSettingsImpl
{
public:
   explicit GeneralSettingsImpl() { SetDefaults(); }

   ~GeneralSettingsImpl() {}

   void SetDefaults()
   {
      debugEnabled_     = kDefaultDebugEnabled_;
      defaultRadarSite_ = kDefaultDefaultRadarSite_;
      fontSizes_        = kDefaultFontSizes_;
      gridWidth_        = kDefaultGridWidth_;
      gridHeight_       = kDefaultGridHeight_;
      mapboxApiKey_     = kDefaultMapboxApiKey_;
   }

   bool                 debugEnabled_;
   std::string          defaultRadarSite_;
   std::vector<int64_t> fontSizes_;
   int64_t              gridWidth_;
   int64_t              gridHeight_;
   std::string          mapboxApiKey_;
};

GeneralSettings::GeneralSettings() : p(std::make_unique<GeneralSettingsImpl>())
{
}
GeneralSettings::~GeneralSettings() = default;

GeneralSettings::GeneralSettings(GeneralSettings&&) noexcept = default;
GeneralSettings&
GeneralSettings::operator=(GeneralSettings&&) noexcept = default;

bool GeneralSettings::debug_enabled() const
{
   return p->debugEnabled_;
}

std::string GeneralSettings::default_radar_site() const
{
   return p->defaultRadarSite_;
}

std::vector<int64_t> GeneralSettings::font_sizes() const
{
   return p->fontSizes_;
}

int64_t GeneralSettings::grid_height() const
{
   return p->gridHeight_;
}

int64_t GeneralSettings::grid_width() const
{
   return p->gridWidth_;
}

std::string GeneralSettings::mapbox_api_key() const
{
   return p->mapboxApiKey_;
}

boost::json::value GeneralSettings::ToJson() const
{
   boost::json::object json;

   json["debug_enabled"]      = p->debugEnabled_;
   json["default_radar_site"] = p->defaultRadarSite_;
   json["font_sizes"]         = boost::json::value_from(p->fontSizes_);
   json["grid_width"]         = p->gridWidth_;
   json["grid_height"]        = p->gridHeight_;
   json["mapbox_api_key"]     = p->mapboxApiKey_;

   return json;
}

std::shared_ptr<GeneralSettings> GeneralSettings::Create()
{
   std::shared_ptr<GeneralSettings> generalSettings =
      std::make_shared<GeneralSettings>();

   return generalSettings;
}

std::shared_ptr<GeneralSettings>
GeneralSettings::Load(const boost::json::value* json, bool& jsonDirty)
{
   std::shared_ptr<GeneralSettings> generalSettings =
      std::make_shared<GeneralSettings>();

   if (json != nullptr && json->is_object())
   {
      jsonDirty |= !util::json::FromJsonBool(json->as_object(),
                                             "debug_enabled",
                                             generalSettings->p->debugEnabled_,
                                             kDefaultDebugEnabled_);
      jsonDirty |=
         !util::json::FromJsonString(json->as_object(),
                                     "default_radar_site",
                                     generalSettings->p->defaultRadarSite_,
                                     kDefaultDefaultRadarSite_);
      jsonDirty |=
         !util::json::FromJsonInt64Array(json->as_object(),
                                         "font_sizes",
                                         generalSettings->p->fontSizes_,
                                         kDefaultFontSizes_,
                                         kFontSizeMinimum_,
                                         kFontSizeMaximum_);
      jsonDirty |= !util::json::FromJsonInt64(json->as_object(),
                                              "grid_width",
                                              generalSettings->p->gridWidth_,
                                              kDefaultGridWidth_,
                                              kGridWidthMinimum_,
                                              kGridWidthMaximum_);
      jsonDirty |= !util::json::FromJsonInt64(json->as_object(),
                                              "grid_height",
                                              generalSettings->p->gridHeight_,
                                              kDefaultGridHeight_,
                                              kGridHeightMinimum_,
                                              kGridHeightMaximum_);
      jsonDirty |=
         !util::json::FromJsonString(json->as_object(),
                                     "mapbox_api_key",
                                     generalSettings->p->mapboxApiKey_,
                                     kDefaultMapboxApiKey_,
                                     1);
   }
   else
   {
      if (json == nullptr)
      {
         logger_->warn("Key is not present, resetting to defaults");
      }
      else if (!json->is_object())
      {
         logger_->warn("Invalid json, resetting to defaults");
      }

      generalSettings->p->SetDefaults();
      jsonDirty = true;
   }

   return generalSettings;
}

bool operator==(const GeneralSettings& lhs, const GeneralSettings& rhs)
{
   return (lhs.p->debugEnabled_ == rhs.p->debugEnabled_ &&
           lhs.p->defaultRadarSite_ == rhs.p->defaultRadarSite_ &&
           lhs.p->fontSizes_ == rhs.p->fontSizes_ &&
           lhs.p->gridWidth_ == rhs.p->gridWidth_ &&
           lhs.p->gridHeight_ == rhs.p->gridHeight_ &&
           lhs.p->mapboxApiKey_ == rhs.p->mapboxApiKey_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
