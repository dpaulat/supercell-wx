#define _USE_MATH_DEFINES
#include <cmath>

#include <scwx/provider/gfs_provider.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/json.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <boost/json.hpp>
#include <cpr/cpr.h>

namespace scwx::provider
{

static const std::string logPrefix_ = "scwx::provider::gfs_provider";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static const std::string kOpenMeteoBaseUrl_ =
   "https://api.open-meteo.com/v1/gfs";

static const std::vector<double> kPressureLevels_ = {
   1000.0, 975.0, 950.0, 925.0, 900.0, 875.0, 850.0, 825.0, 800.0, 775.0, 750.0,
   700.0,  650.0, 600.0, 550.0, 500.0, 450.0, 400.0, 350.0, 300.0, 250.0, 200.0,
   175.0,  150.0, 125.0, 100.0, 70.0,  50.0,  30.0,  20.0,  10.0};

class GfsProvider::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   std::string BuildUrl(double lat, double lon, int cycle, int fhr)
   {
      int targetHour   = cycle + fhr;
      int forecastDays = std::max(1, std::min(targetHour / 24 + 1, 16));

      std::ostringstream url;
      url << kOpenMeteoBaseUrl_ << "?latitude=" << std::fixed
          << std::setprecision(4) << lat << "&longitude=" << std::fixed
          << std::setprecision(4) << lon << "&hourly=";

      auto addVars = [&](const std::string& prefix)
      {
         for (size_t i = 0; i < kPressureLevels_.size(); ++i)
         {
            if (i > 0)
            {
               url << ",";
            }
            url << prefix << static_cast<int>(kPressureLevels_[i]) << "hPa";
         }
      };

      addVars("temperature_");
      url << ",";
      addVars("relative_humidity_");
      url << ",";
      addVars("wind_speed_");
      url << ",";
      addVars("wind_direction_");
      url << ",";
      addVars("geopotential_height_");

      url << "&forecast_days=" << forecastDays << "&temperature_unit=celsius"
          << "&wind_speed_unit=ms";

      return url.str();
   }

   static double DewpointFromRH(double t_C, double rh_pct)
   {
      if (rh_pct <= 0.0)
      {
         return -999.0;
      }
      double e =
         (rh_pct / 100.0) * 6.112 * std::exp((17.67 * t_C) / (t_C + 243.5));
      double td = (243.5 * std::log(e / 6.112)) / (17.67 - std::log(e / 6.112));
      return td;
   }

   static std::optional<double> JsonGetOptional(const boost::json::object& obj,
                                                const std::string&         key,
                                                std::size_t index)
   {
      auto it = obj.find(key);
      if (it == obj.end() || !it->value().is_array())
      {
         return std::nullopt;
      }
      const auto& arr = it->value().as_array();
      if (index >= arr.size() || arr[index].is_null())
      {
         return std::nullopt;
      }
      if (arr[index].is_double())
      {
         return arr[index].as_double();
      }
      if (arr[index].is_int64())
      {
         return static_cast<double>(arr[index].as_int64());
      }
      return std::nullopt;
   }

   std::optional<std::shared_ptr<sounding::SoundingData>>
   FetchSounding(double lat, double lon, int cycle, int fhr)
   {
      logger_->info("Fetching GFS sounding: lat={}, lon={}, cycle={}, fhr={}",
                    lat,
                    lon,
                    cycle,
                    fhr);

      std::string url = BuildUrl(lat, lon, cycle, fhr);
      logger_->debug("Open-Meteo URL: {}", url);

      auto response = cpr::Get(cpr::Url {url},
                               network::cpr::GetDefaultTimeout(),
                               network::cpr::GetDefaultConnectTimeout(),
                               network::cpr::GetDefaultLowSpeed());

      if (response.status_code != 200)
      {
         logger_->warn("Open-Meteo request failed: HTTP {}",
                       response.status_code);
         return std::nullopt;
      }

      boost::json::value json = util::json::ReadJsonString(response.text);
      if (json.is_null())
      {
         logger_->warn("Failed to parse Open-Meteo JSON response");
         return std::nullopt;
      }

      if (!json.is_object())
      {
         logger_->warn("Open-Meteo response is not a JSON object");
         return std::nullopt;
      }

      const auto& root = json.as_object();

      // Check for API error
      auto errorIt = root.find("error");
      if (errorIt != root.end() && errorIt->value().is_bool() &&
          errorIt->value().as_bool())
      {
         std::string reason   = "Unknown error";
         auto        reasonIt = root.find("reason");
         if (reasonIt != root.end() && reasonIt->value().is_string())
         {
            reason = reasonIt->value().as_string().c_str();
         }
         logger_->warn("Open-Meteo API error: {}", reason);
         return std::nullopt;
      }

      auto hourlyIt = root.find("hourly");
      if (hourlyIt == root.end() || !hourlyIt->value().is_object())
      {
         logger_->warn("Open-Meteo response missing 'hourly' data");
         return std::nullopt;
      }
      const auto& hourly = hourlyIt->value().as_object();

      auto timeIt = hourly.find("time");
      if (timeIt == hourly.end() || !timeIt->value().is_array() ||
          timeIt->value().as_array().empty())
      {
         logger_->warn("Open-Meteo response missing time array");
         return std::nullopt;
      }
      const auto& times = timeIt->value().as_array();

      // Compute target UTC time string: YYYY-MM-DDTHH:00
      int  targetHourOffset = cycle + fhr;
      auto now              = std::chrono::system_clock::now();
      auto todayDay         = std::chrono::floor<std::chrono::days>(now);
      auto target           = todayDay + std::chrono::hours(targetHourOffset);
      std::chrono::year_month_day ymd {
         std::chrono::floor<std::chrono::days>(target)};
      int targetHourOfDay = targetHourOffset % 24;

      std::ostringstream targetTimeStr;
      targetTimeStr << std::setfill('0') << std::setw(4)
                    << static_cast<int>(ymd.year()) << "-" << std::setw(2)
                    << static_cast<unsigned>(ymd.month()) << "-" << std::setw(2)
                    << static_cast<unsigned>(ymd.day()) << "T" << std::setw(2)
                    << targetHourOfDay << ":00";
      std::string targetTime = targetTimeStr.str();

      // Find matching time index
      int timeIndex = -1;
      for (size_t i = 0; i < times.size(); ++i)
      {
         if (times[i].is_string() && times[i].as_string() == targetTime)
         {
            timeIndex = static_cast<int>(i);
            break;
         }
      }

      if (timeIndex < 0)
      {
         logger_->warn(
            "Target time {} not found in Open-Meteo response "
            "({} entries)",
            targetTime,
            times.size());
         return std::nullopt;
      }

      // Build sounding
      auto sounding = std::make_shared<sounding::SoundingData>();
      sounding->set_latitude(lat);
      sounding->set_longitude(lon);

      std::ostringstream id;
      id << "GFS_" << cycle << "Z_F" << std::setw(3) << std::setfill('0')
         << fhr;
      sounding->set_station_id(id.str());
      sounding->set_forecast_time(target);

      int successCount = 0;

      for (double pressure : kPressureLevels_)
      {
         int         level  = static_cast<int>(pressure);
         std::string suffix = std::to_string(level) + "hPa";

         auto temp = JsonGetOptional(hourly,
                                     "temperature_" + suffix,
                                     static_cast<std::size_t>(timeIndex));
         auto rh   = JsonGetOptional(hourly,
                                   "relative_humidity_" + suffix,
                                   static_cast<std::size_t>(timeIndex));
         auto ws   = JsonGetOptional(hourly,
                                   "wind_speed_" + suffix,
                                   static_cast<std::size_t>(timeIndex));
         auto wd   = JsonGetOptional(hourly,
                                   "wind_direction_" + suffix,
                                   static_cast<std::size_t>(timeIndex));
         auto hgt  = JsonGetOptional(hourly,
                                    "geopotential_height_" + suffix,
                                    static_cast<std::size_t>(timeIndex));

         if (!temp.has_value() || !rh.has_value() || !ws.has_value() ||
             !wd.has_value() || !hgt.has_value())
         {
            continue;
         }

         double td = DewpointFromRH(*temp, *rh);

         sounding::SoundingLevel sl {};
         sl.pressure_hPa_       = pressure;
         sl.temperature_C_      = *temp;
         sl.dewpoint_C_         = td;
         sl.wind_speed_mps_     = *ws;
         sl.wind_direction_deg_ = *wd;
         sl.height_m_           = *hgt;

         sounding->add_level(sl);
         successCount++;
      }

      logger_->info("GFS sounding complete: {} pressure levels retrieved",
                    successCount);

      if (successCount < 5)
      {
         logger_->warn("Insufficient levels for a meaningful sounding ({})",
                       successCount);
         return std::nullopt;
      }

      sounding->compute_derived();
      return sounding;
   }

   std::atomic<bool> running_ {true};
};

GfsProvider::GfsProvider() : p(std::make_unique<Impl>()) {}
GfsProvider::~GfsProvider() = default;

GfsProvider::GfsProvider(GfsProvider&&) noexcept            = default;
GfsProvider& GfsProvider::operator=(GfsProvider&&) noexcept = default;

std::optional<std::shared_ptr<sounding::SoundingData>>
GfsProvider::FetchSounding(double lat, double lon, int cycle, int fhr)
{
   return p->FetchSounding(lat, lon, cycle, fhr);
}

void GfsProvider::Shutdown()
{
   p->running_ = false;
}

} // namespace scwx::provider
