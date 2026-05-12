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
   double pressure_hPa_ {};
   double temperature_C_ {};
   double dewpoint_C_ {};
   double wind_speed_mps_ {};
   double wind_direction_deg_ {};
   double height_m_ {};
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

   double                                latitude() const;
   double                                longitude() const;
   const std::string&                    station_id() const;
   std::chrono::system_clock::time_point forecast_time() const;
   const std::vector<SoundingLevel>&     levels() const;
   const std::vector<SoundingLevel>&     parcel_profile() const;

   void set_latitude(double lat);
   void set_longitude(double lon);
   void set_station_id(const std::string& id);
   void set_forecast_time(std::chrono::system_clock::time_point t);
   void set_levels(const std::vector<SoundingLevel>& levels);
   void add_level(const SoundingLevel& level);

   double sbcape_jkg() const;
   double sbcin_jkg() const;
   double mlcape_jkg() const;
   double mlcin_jkg() const;
   double mucape_jkg() const;
   double mucin_jkg() const;

   double lcl_pressure_hPa() const;
   double lcl_temperature_C() const;
   double lfc_pressure_hPa() const;
   double lfc_temperature_C() const;
   double el_pressure_hPa() const;
   double el_temperature_C() const;

   double bulk_shear_mps(double lower_km, double upper_km) const;
   double storm_relative_helicity_m2s2(double lower_km, double upper_km) const;

   double lapse_rate_c_km(double lower_km, double upper_km) const;
   double precipitable_water_mm() const;

   double significant_tornado_parameter() const;
   double supercell_composite_parameter() const;
   double significant_hail_parameter() const;
   double energy_helicity_index() const;

   void compute_derived();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::sounding
