#include <scwx/qt/settings/settings_variable_base.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

static const std::string logPrefix_ =
   "scwx::qt::settings::settings_variable_base";

class SettingsVariableBase::Impl
{
public:
   explicit Impl(const std::string& name) : name_ {name} {}

   ~Impl() {}

   const std::string name_;

   boost::signals2::signal<void()> changedSignal_ {};
   boost::signals2::signal<void()> stagedSignal_ {};
};

SettingsVariableBase::SettingsVariableBase(const std::string& name) :
    p(std::make_unique<Impl>(name))
{
}

SettingsVariableBase::~SettingsVariableBase() = default;

SettingsVariableBase::SettingsVariableBase(SettingsVariableBase&&) noexcept =
   default;

SettingsVariableBase&
SettingsVariableBase::operator=(SettingsVariableBase&&) noexcept = default;

std::string SettingsVariableBase::name() const
{
   return p->name_;
}

boost::signals2::signal<void()>& SettingsVariableBase::changed_signal()
{
   return p->changedSignal_;
}

boost::signals2::signal<void()>& SettingsVariableBase::staged_signal()
{
   return p->stagedSignal_;
}

bool SettingsVariableBase::Equals(const SettingsVariableBase& o) const
{
   return p->name_ == o.p->name_;
}

bool operator==(const SettingsVariableBase& lhs,
                const SettingsVariableBase& rhs)
{
   return typeid(lhs) == typeid(rhs) && lhs.Equals(rhs);
}

} // namespace settings
} // namespace qt
} // namespace scwx
