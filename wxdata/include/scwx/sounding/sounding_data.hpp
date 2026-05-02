#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace scwx::sounding
{

struct SoundingLevel
{
   double pressure_hPa_    {};
   double temperature_C_   {};
   double dewpoint_C_      {};
   double wind_speed_mps_  {};
   double wind_direction_deg_ {};
   double height_m_        {};
};

class SoundingData
{
public:
   explicit SoundingData();
   ~SoundingData();

   SoundingData(const SoundingData&)            = delete;
   SoundingData& operator=(const SoundingData&) = delete;

   SoundingData(SoundingData&&) noexcept;
   SoundingData& operator=(SoundingData&&) noexcept;

   double latitude() const;
   double longitude() const;
   const std::string& station_id() const;
   std::chrono::system_clock::time_point forecast_time() const;
   const std::vector<SoundingLevel>& levels() const;

   void set_latitude(double lat);
   void set_longitude(double lon);
   void set_station_id(const std::string& id);
   void set_forecast_time(std::chrono::system_clock::time_point t);
   void set_levels(const std::vector<SoundingLevel>& levels);
   void add_level(const SoundingLevel& level);

   double cape_jkg() const;
   double cin_jkg() const;
   double lcl_pressure_hPa() const;
   double lcl_temperature_C() const;
   double lfc_pressure_hPa() const;
   double lfc_temperature_C() const;
   double el_pressure_hPa() const;
   double el_temperature_C() const;

   double surface_based_shear_s_1(double lower_km = 1.0,
                                   double upper_km = 6.0) const;
   double storm_relative_helicity_m2s2(double lower_km = 0.0,
                                        double upper_km = 3.0) const;

   void compute_derived();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::sounding
