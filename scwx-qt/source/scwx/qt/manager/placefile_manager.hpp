#pragma once

#include <scwx/gr/placefile.hpp>
#include <scwx/qt/config/radar_site.hpp>
#include <scwx/qt/types/font_types.hpp>
#include <scwx/qt/types/imgui_font.hpp>

#include <istream>
#include <ostream>

#include <QObject>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

class PlacefileManager : public QObject
{
   Q_OBJECT

public:
   explicit PlacefileManager();
   ~PlacefileManager();

   using FontMap =
      boost::unordered_flat_map<std::size_t,
                                std::pair<std::shared_ptr<types::ImGuiFont>,
                                          units::font_size::pixels<float>>>;

   bool        placefile_enabled(const std::string& name);
   bool        placefile_thresholded(const std::string& name);
   std::string placefile_title(const std::string& name);
   std::shared_ptr<gr::Placefile> placefile(const std::string& name);
   FontMap                        placefile_fonts(const std::string& name);

   void set_placefile_enabled(const std::string& name, bool enabled);
   void set_placefile_thresholded(const std::string& name, bool thresholded);
   void set_placefile_url(const std::string& name, const std::string& newUrl);

   void SetRadarSite(std::shared_ptr<config::RadarSite> radarSite);

   void AddUrl(const std::string& urlString,
               const std::string& title       = {},
               bool               enabled     = false,
               bool               thresholded = false);
   void RemoveUrl(const std::string& urlString);
   void ReadPlacefileSettings(std::istream& is);
   void WritePlacefileSettings(std::ostream& os);

   void Refresh(const std::string& name);

   static std::shared_ptr<PlacefileManager> Instance();

signals:
   void PlacefilesInitialized();
   void PlacefileEnabled(const std::string& name, bool enabled);
   void PlacefileRemoved(const std::string& name);
   void PlacefileRenamed(const std::string& oldName,
                         const std::string& newName);
   void PlacefileUpdated(const std::string& name);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
