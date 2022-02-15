#pragma once

#include <memory>
#include <string>

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

   RadarSite(const RadarSite&) = delete;
   RadarSite& operator=(const RadarSite&) = delete;

   RadarSite(RadarSite&&) noexcept;
   RadarSite& operator=(RadarSite&&) noexcept;

   std::string type() const;
   std::string id() const;
   double      latitude() const;
   double      longitude() const;
   std::string country() const;
   std::string state() const;
   std::string place() const;

   static std::shared_ptr<RadarSite> Get(const std::string& id);

   static void   Initialize();
   static size_t ReadConfig(const std::string& path);

private:
   std::unique_ptr<RadarSiteImpl> p;
};

std::string GetRadarIdFromSiteId(const std::string& siteId);

} // namespace config
} // namespace qt
} // namespace scwx
