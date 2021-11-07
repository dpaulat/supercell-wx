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

class PaletteSettingsImpl;

class PaletteSettings
{
public:
   explicit PaletteSettings();
   ~PaletteSettings();

   PaletteSettings(const PaletteSettings&) = delete;
   PaletteSettings& operator=(const PaletteSettings&) = delete;

   PaletteSettings(PaletteSettings&&) noexcept;
   PaletteSettings& operator=(PaletteSettings&&) noexcept;

   const std::string& palette(const std::string& name) const;

   boost::json::value ToJson() const;

   static std::shared_ptr<PaletteSettings> Create();
   static std::shared_ptr<PaletteSettings> Load(const boost::json::value* json,
                                                bool& jsonDirty);

   friend bool operator==(const PaletteSettings& lhs,
                          const PaletteSettings& rhs);

private:
   std::unique_ptr<PaletteSettingsImpl> p;
};

} // namespace settings
} // namespace qt
} // namespace scwx
