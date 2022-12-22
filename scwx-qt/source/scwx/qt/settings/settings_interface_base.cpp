#include <scwx/qt/settings/settings_interface_base.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::settings_interface_base";

class SettingsInterfaceBase::Impl
{
public:
   explicit Impl() {}
   ~Impl() {}
};

SettingsInterfaceBase::SettingsInterfaceBase() : p(std::make_unique<Impl>()) {}

SettingsInterfaceBase::~SettingsInterfaceBase() = default;

SettingsInterfaceBase::SettingsInterfaceBase(SettingsInterfaceBase&&) noexcept =
   default;

SettingsInterfaceBase&
SettingsInterfaceBase::operator=(SettingsInterfaceBase&&) noexcept = default;

} // namespace settings
} // namespace qt
} // namespace scwx
