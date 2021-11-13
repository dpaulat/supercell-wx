#pragma once

#include <scwx/wsr88d/rda/digital_radar_data.hpp>
#include <scwx/wsr88d/rda/volume_coverage_pattern_data.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace scwx
{
namespace wsr88d
{

class Ar2vFileImpl;

/**
 * @brief The Archive II file is specified in the Interface Control Document for
 * the Archive II/User, Document Number 2620010H, published by the WSR-88D Radar
 * Operations Center.
 */
class Ar2vFile
{
public:
   explicit Ar2vFile();
   ~Ar2vFile();

   Ar2vFile(const Ar2vFile&) = delete;
   Ar2vFile& operator=(const Ar2vFile&) = delete;

   Ar2vFile(Ar2vFile&&) noexcept;
   Ar2vFile& operator=(Ar2vFile&&) noexcept;

   uint32_t julian_date() const;
   uint32_t milliseconds() const;

   std::chrono::system_clock::time_point start_time() const;
   std::chrono::system_clock::time_point end_time() const;

   std::map<uint16_t,
            std::map<uint16_t, std::shared_ptr<rda::DigitalRadarData>>>
                                                         radar_data() const;
   std::shared_ptr<const rda::VolumeCoveragePatternData> vcp_data() const;

   bool LoadFile(const std::string& filename);

private:
   std::unique_ptr<Ar2vFileImpl> p;
};

} // namespace wsr88d
} // namespace scwx
