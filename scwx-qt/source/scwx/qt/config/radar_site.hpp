#pragma once

#include <scwx/util/time.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace scwx
{
namespace qt
{
namespace config
{

class RadarSiteImpl;

class RadarSite
{
public:
   explicit RadarSite();
   ~RadarSite();

   RadarSite(const RadarSite&)            = delete;
   RadarSite& operator=(const RadarSite&) = delete;

   RadarSite(RadarSite&&) noexcept;
   RadarSite& operator=(RadarSite&&) noexcept;

   std::string type() const;
   std::string type_name() const;
   std::string id() const;
   double      latitude() const;
   double      longitude() const;
   std::string country() const;
   std::string state() const;
   std::string place() const;
   std::string location_name() const;
   std::string tz_name() const;

   const scwx::util::time_zone* time_zone() const;

   static std::shared_ptr<RadarSite>              Get(const std::string& id);
   static std::vector<std::shared_ptr<RadarSite>> GetAll();

   /**
    * Find the nearest radar site to the supplied location.
    *
    * @param latitude Latitude in degrees
    * @param longitude Longitude in degrees
    * @param type Restrict results to optional radar type
    *
    * @return Nearest radar site
    */
   static std::shared_ptr<RadarSite>
   FindNearest(double                     latitude,
               double                     longitude,
               std::optional<std::string> type = std::nullopt);

   static void   Initialize();
   static size_t ReadConfig(const std::string& path);

private:
   std::unique_ptr<RadarSiteImpl> p;
};

std::string GetRadarIdFromSiteId(const std::string& siteId);

} // namespace config
} // namespace qt
} // namespace scwx
