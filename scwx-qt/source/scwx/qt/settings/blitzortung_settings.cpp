#include <scwx/qt/settings/blitzortung_settings.hpp>

namespace scwx::qt::settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::blitzortung_settings";

class BlitzortungSettings::Impl
{
public:
   explicit Impl() { enabled_.SetDefault(false); }

   ~Impl()                      = default;
   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   SettingsVariable<bool> enabled_ {"blitzortung_enabled"};
};

BlitzortungSettings::BlitzortungSettings() :
    SettingsCategory("blitzortung"), p(std::make_unique<Impl>())
{
   RegisterVariables({&p->enabled_});
   SetDefaults();
}
BlitzortungSettings::~BlitzortungSettings() = default;

BlitzortungSettings::BlitzortungSettings(BlitzortungSettings&&) noexcept =
   default;
BlitzortungSettings&
BlitzortungSettings::operator=(BlitzortungSettings&&) noexcept = default;

SettingsVariable<bool>& BlitzortungSettings::enabled() const
{
   return p->enabled_;
}

BlitzortungSettings& BlitzortungSettings::Instance()
{
   static BlitzortungSettings blitzortungSettings_;
   return blitzortungSettings_;
}

bool operator==(const BlitzortungSettings& lhs, const BlitzortungSettings& rhs)
{
   return (lhs.p->enabled_ == rhs.p->enabled_);
}

} // namespace scwx::qt::settings
