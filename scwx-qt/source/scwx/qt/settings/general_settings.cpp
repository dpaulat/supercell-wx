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

static const std::string DEFAULT_DEFAULT_RADAR_SITE = "KLSX";
static const int64_t     DEFAULT_GRID_WIDTH         = 1;
static const int64_t     DEFAULT_GRID_HEIGHT        = 1;

static const int64_t GRID_WIDTH_MINIMUM  = 1;
static const int64_t GRID_WIDTH_MAXIMUM  = 2;
static const int64_t GRID_HEIGHT_MINIMUM = 1;
static const int64_t GRID_HEIGHT_MAXIMUM = 2;

class GeneralSettingsImpl
{
public:
   explicit GeneralSettingsImpl() {}

   ~GeneralSettingsImpl() {}

   void SetDefaults()
   {
      defaultRadarSite_ = DEFAULT_DEFAULT_RADAR_SITE;
      gridWidth_        = DEFAULT_GRID_WIDTH;
      gridHeight_       = DEFAULT_GRID_HEIGHT;
   }

   std::string defaultRadarSite_;
   int64_t     gridWidth_;
   int64_t     gridHeight_;
};

GeneralSettings::GeneralSettings() : p(std::make_unique<GeneralSettingsImpl>())
{
}
GeneralSettings::~GeneralSettings() = default;

GeneralSettings::GeneralSettings(GeneralSettings&&) noexcept = default;
GeneralSettings&
GeneralSettings::operator=(GeneralSettings&&) noexcept = default;

const std::string& GeneralSettings::default_radar_site() const
{
   return p->defaultRadarSite_;
}

int64_t GeneralSettings::grid_height() const
{
   return p->gridHeight_;
}

int64_t GeneralSettings::grid_width() const
{
   return p->gridWidth_;
}

boost::json::value GeneralSettings::ToJson() const
{
   boost::json::object json;

   json["default_radar_site"] = p->defaultRadarSite_;
   json["grid_width"]         = p->gridWidth_;
   json["grid_height"]        = p->gridHeight_;

   return json;
}

std::shared_ptr<GeneralSettings> GeneralSettings::Create()
{
   std::shared_ptr<GeneralSettings> generalSettings =
      std::make_shared<GeneralSettings>();

   generalSettings->p->SetDefaults();

   return generalSettings;
}

std::shared_ptr<GeneralSettings>
GeneralSettings::Load(const boost::json::value* json, bool& jsonDirty)
{
   std::shared_ptr<GeneralSettings> generalSettings =
      std::make_shared<GeneralSettings>();

   if (json != nullptr && json->is_object())
   {
      jsonDirty |=
         !util::json::FromJsonString(json->as_object(),
                                     "default_radar_site",
                                     generalSettings->p->defaultRadarSite_,
                                     DEFAULT_DEFAULT_RADAR_SITE);
      jsonDirty |= !util::json::FromJsonInt64(json->as_object(),
                                              "grid_width",
                                              generalSettings->p->gridWidth_,
                                              DEFAULT_GRID_WIDTH,
                                              GRID_WIDTH_MINIMUM,
                                              GRID_WIDTH_MAXIMUM);
      jsonDirty |= !util::json::FromJsonInt64(json->as_object(),
                                              "grid_height",
                                              generalSettings->p->gridHeight_,
                                              DEFAULT_GRID_HEIGHT,
                                              GRID_HEIGHT_MINIMUM,
                                              GRID_HEIGHT_MAXIMUM);
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
   return (lhs.p->defaultRadarSite_ == rhs.p->defaultRadarSite_ &&
           lhs.p->gridWidth_ == rhs.p->gridWidth_ &&
           lhs.p->gridHeight_ == rhs.p->gridHeight_);
}

} // namespace settings
} // namespace qt
} // namespace scwx
