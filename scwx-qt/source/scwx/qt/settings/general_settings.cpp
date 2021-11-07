#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/qt/util/json.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ = "[scwx::qt::settings::general_settings] ";

static const std::string DEFAULT_DEFAULT_RADAR_SITE = "KLSX";

class GeneralSettingsImpl
{
public:
   explicit GeneralSettingsImpl() {}

   ~GeneralSettingsImpl() {}

   void SetDefaults() { defaultRadarSite_ = DEFAULT_DEFAULT_RADAR_SITE; }

   std::string defaultRadarSite_;
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

boost::json::value GeneralSettings::ToJson() const
{
   boost::json::object json;

   json["default_radar_site"] = p->defaultRadarSite_;

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
   }
   else
   {
      if (json == nullptr)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Key is not present, resetting to defaults";
      }
      else if (!json->is_object())
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Invalid json, resetting to defaults";
      }

      generalSettings->p->SetDefaults();
      jsonDirty = true;
   }

   return generalSettings;
}

bool operator==(const GeneralSettings& lhs, const GeneralSettings& rhs)
{
   return lhs.p->defaultRadarSite_ == rhs.p->defaultRadarSite_;
}

} // namespace settings
} // namespace qt
} // namespace scwx
