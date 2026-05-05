#pragma once

#include <scwx/qt/settings/settings_category.hpp>
#include <scwx/qt/settings/settings_variable.hpp>

#include <memory>

namespace scwx::qt::settings
{

class SpcOutlookSettings : public SettingsCategory
{
public:
   explicit SpcOutlookSettings();
   ~SpcOutlookSettings() override;

   SpcOutlookSettings(const SpcOutlookSettings&)            = delete;
   SpcOutlookSettings& operator=(const SpcOutlookSettings&) = delete;

   SpcOutlookSettings(SpcOutlookSettings&&) noexcept;
   SpcOutlookSettings& operator=(SpcOutlookSettings&&) noexcept;

   SettingsVariable<std::string>& selected_day();
   SettingsVariable<std::string>& selected_product();
   SettingsVariable<int>&         opacity();
   SettingsVariable<bool>&        auto_refresh();

   bool ReadJson(const boost::json::object& json) override;
   void WriteJson(boost::json::object& json) const override;

   static SpcOutlookSettings& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::settings
