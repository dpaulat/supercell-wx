#pragma once

#include <memory>
#include <string>

#include <boost/json.hpp>

namespace scwx
{
namespace qt
{
namespace settings
{

class GeneralSettingsImpl;

class GeneralSettings
{
public:
   explicit GeneralSettings();
   ~GeneralSettings();

   GeneralSettings(const GeneralSettings&) = delete;
   GeneralSettings& operator=(const GeneralSettings&) = delete;

   GeneralSettings(GeneralSettings&&) noexcept;
   GeneralSettings& operator=(GeneralSettings&&) noexcept;

   const std::string& default_radar_site();

   boost::json::value ToJson();

   static std::shared_ptr<GeneralSettings> Create();
   static std::shared_ptr<GeneralSettings> Load(const boost::json::value* json,
                                                bool& jsonDirty);

   friend bool operator==(const GeneralSettings& lhs,
                          const GeneralSettings& rhs);

private:
   std::unique_ptr<GeneralSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
