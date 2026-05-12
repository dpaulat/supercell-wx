#define _USE_MATH_DEFINES
#include <cmath>

#include <scwx/sounding/sounding_data.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <limits>

namespace scwx::sounding
{

static const std::string logPrefix_ = "scwx::sounding::sounding_data";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// Thermodynamic constants
static constexpr double kRd = 287.058; // J/(kg·K) - dry air gas constant
static constexpr double kCp =
   1005.0; // J/(kg·K) - specific heat of dry air at constant pressure
static constexpr double kRv = 461.5; // J/(kg·K) - water vapor gas constant
static constexpr double kLv =
   2.501e6; // J/kg - latent heat of vaporization at 0°C
static constexpr double kGamma   = kRd / kCp; // 0.2854...
static constexpr double kEpsilon = kRd / kRv; // 0.622...

class SoundingData::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   // Saturation vapor pressure (hPa) using Magnus formula
   static double Es(double t_C)
   {
      return 6.112 * std::exp((17.67 * t_C) / (t_C + 243.5));
   }

   // Mixing ratio (g/kg) from T and RH
   static double MixingRatio(double t_C, double rh_pct)
   {
      double e = rh_pct / 100.0 * Es(t_C);
      return 1000.0 * kEpsilon * e /
             100.0; // simplified: w = ε*e/p, using p=1000hPa approx
   }

   // LCL temperature using Bolton's formula
   static double LclTemperature(double t_C, double td_C)
   {
      double tk  = t_C + 273.15;
      double tdk = td_C + 273.15;
      // Bolton (1980): 1/T_LCL = 1/(T_d-56) + ln(T/T_d)/800
      double inv_tlcl = 1.0 / (tdk - 56.0) + std::log(tk / tdk) / 800.0;
      return (1.0 / inv_tlcl) - 273.15;
   }

   // LCL pressure using the dry adiabat
   static double LclPressure(double p_sfc, double t_C, double td_C)
   {
      double tk   = t_C + 273.15;
      double tlcl = LclTemperature(t_C, td_C) + 273.15;
      // Dry adiabatic: T_parcel * p^(-gamma) = const
      return p_sfc * std::pow(tlcl / tk, 1.0 / kGamma);
   }

   // Virtual temperature
   static double VirtualTemperature(double t_C, double mixing_ratio_gpkg)
   {
      double tk = t_C + 273.15;
      double w  = mixing_ratio_gpkg / 1000.0;
      return tk * (1.0 + w / kEpsilon) / (1.0 + w);
   }

   // Dry adiabatic temperature at new pressure
   static double DryAdiabatT(double t_start_C, double p_start, double p_end)
   {
      double tk = t_start_C + 273.15;
      return tk * std::pow(p_end / p_start, kGamma) - 273.15;
   }

   // Moist adiabatic temperature at new pressure (pseudo-adiabatic
   // approximation) Uses iterative method
   static double MoistAdiabatT(double t_start_C, double p_start, double p_end)
   {
      if (p_end >= p_start)
      {
         return t_start_C;
      }

      double tk = t_start_C + 273.15;
      double p  = p_start;
      double dp = (p_end - p_start) / 50.0; // 50 steps for integration
      if (dp >= 0.0)
      {
         dp = -1.0;
      }

      for (int step = 0;
           step < 50 && ((dp < 0 && p > p_end) || (dp > 0 && p < p_end));
           ++step)
      {
         double es             = Es(tk - 273.15);
         double ws             = kEpsilon * es / (p - es);
         double dlnT_over_dlnp = kRd / kCp * (1.0 + kLv * ws / (kRd * tk)) /
                                 (1.0 + kLv * kLv * ws / (kCp * kRv * tk * tk));

         double t_old = tk;
         tk += dlnT_over_dlnp * tk / p * dp;
         p += dp;

         // Clamp in case of numerical instability
         if (std::abs(tk - t_old) > 50.0 || tk < 100.0 || tk > 400.0)
         {
            tk = t_old;
            break;
         }
      }

      return tk - 273.15;
   }

   // Integrate a parcel from its starting point up to the top of the sounding
   struct ParcelResult
   {
      double                capeJkg {};
      double                cinJkg {};
      std::optional<double> lclPressure {};
      std::optional<double> lclTemperature {};
      std::optional<double> lfcPressure {};
      std::optional<double> lfcTemperature {};
      std::optional<double> elPressure {};
      std::optional<double> elTemperature {};
   };

   ParcelResult IntegrateParcel(double                      p_start,
                                double                      t_start,
                                double                      td_start,
                                std::vector<SoundingLevel>* profile = nullptr)
   {
      ParcelResult res;
      if (levels_.empty())
         return res;

      res.lclPressure    = LclPressure(p_start, t_start, td_start);
      res.lclTemperature = LclTemperature(t_start, td_start);

      if (profile)
      {
         profile->clear();
         profile->push_back({p_start, t_start, td_start, 0.0, 0.0, 0.0});
      }

      auto sorted = levels_;
      std::sort(sorted.begin(),
                sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      bool   aboveLcl   = false;
      bool   crossedLfc = false;
      double capeInt    = 0.0;
      double cinInt     = 0.0;

      // Start integration from the parcel's origin pressure
      for (size_t i = 1; i < sorted.size(); ++i)
      {
         double p_upper = sorted[i].pressure_hPa_;
         double p_lower = sorted[i - 1].pressure_hPa_;

         // Only integrate levels above the parcel start
         if (p_lower > p_start)
         {
            if (p_upper > p_start)
               continue;
            p_lower = p_start; // Clip to parcel start
         }

         double dp = p_lower - p_upper;
         if (dp <= 0)
            continue;

         double p_mid = 0.5 * (p_lower + p_upper);
         double t_env =
            0.5 * (sorted[i].temperature_C_ + sorted[i - 1].temperature_C_);

         // Parcel temperature
         double t_parcel;
         if (!aboveLcl && p_mid > *res.lclPressure)
         {
            t_parcel = DryAdiabatT(t_start, p_start, p_mid);
         }
         else
         {
            if (!aboveLcl)
               aboveLcl = true;
            double t_at_lcl = DryAdiabatT(t_start, p_start, *res.lclPressure);
            t_parcel        = MoistAdiabatT(t_at_lcl, *res.lclPressure, p_mid);
         }

         if (profile)
         {
            profile->push_back({p_mid, t_parcel, td_start, 0.0, 0.0, 0.0});
         }

         double tv_parcel = VirtualTemperature(t_parcel, 0.0);
         double tv_env    = VirtualTemperature(t_env, 0.0);
         double dlnp      = std::log(p_lower / p_upper);
         double integrand = kRd * (tv_parcel - tv_env) * dlnp;

         if (integrand > 0)
         {
            capeInt += integrand;
            if (!crossedLfc)
            {
               crossedLfc         = true;
               res.lfcPressure    = p_mid;
               res.lfcTemperature = t_env;
            }
         }
         else
         {
            cinInt += integrand;
         }

         // EL check
         if (crossedLfc && tv_parcel < tv_env && !res.elPressure)
         {
            res.elPressure    = p_mid;
            res.elTemperature = t_env;
         }
      }

      res.capeJkg = capeInt;
      res.cinJkg  = std::abs(cinInt);

      if (crossedLfc && !res.elPressure)
      {
         res.elPressure    = sorted.back().pressure_hPa_;
         res.elTemperature = sorted.back().temperature_C_;
      }

      return res;
   }

   void ComputeCapeCin()
   {
      capeCalculated_ = true;
      if (levels_.empty())
         return;

      auto sorted = levels_;
      std::sort(sorted.begin(),
                sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      // 1. Surface-Based Parcel
      const auto& sfc = sorted[0];
      auto        sb  = IntegrateParcel(sfc.pressure_hPa_,
                                sfc.temperature_C_,
                                sfc.dewpoint_C_,
                                &parcel_profile_);
      sbcapeJkg_      = sb.capeJkg;
      sbcinJkg_       = sb.cinJkg;
      lclPressure_    = sb.lclPressure;
      lclTemperature_ = sb.lclTemperature;
      lfcPressure_    = sb.lfcPressure;
      lfcTemperature_ = sb.lfcTemperature;
      elPressure_     = sb.elPressure;
      elTemperature_  = sb.elTemperature;

      // 2. Mixed-Layer Parcel (Lowest 100mb mean)
      double p_limit = sfc.pressure_hPa_ - 100.0;
      double sumT = 0, sumTd = 0, count = 0;
      for (const auto& lvl : sorted)
      {
         if (lvl.pressure_hPa_ < p_limit)
            break;
         sumT += lvl.temperature_C_;
         sumTd += lvl.dewpoint_C_;
         count++;
      }
      if (count > 0)
      {
         auto ml =
            IntegrateParcel(sfc.pressure_hPa_, sumT / count, sumTd / count);
         mlcapeJkg_ = ml.capeJkg;
         mlcinJkg_  = ml.cinJkg;
      }

      // 3. Most-Unstable Parcel (Highest MU CAPE in lowest 300mb)
      p_limit    = sfc.pressure_hPa_ - 300.0;
      mucapeJkg_ = 0;
      mucinJkg_  = 0;
      for (const auto& lvl : sorted)
      {
         if (lvl.pressure_hPa_ < p_limit)
            break;
         auto mu = IntegrateParcel(
            lvl.pressure_hPa_, lvl.temperature_C_, lvl.dewpoint_C_);
         if (mu.capeJkg > mucapeJkg_)
         {
            mucapeJkg_ = mu.capeJkg;
            mucinJkg_  = mu.cinJkg;
         }
      }
   }

   // Bulk shear magnitude (m/s) between two heights (m)
   double BulkShear(double lower_m, double upper_m) const
   {
      if (levels_.size() < 2)
         return 0.0;

      auto sorted = levels_;
      std::sort(sorted.begin(),
                sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      auto getWind = [&](double h_target) -> std::pair<double, double>
      {
         for (size_t i = 1; i < sorted.size(); ++i)
         {
            if (sorted[i - 1].height_m_ <= h_target &&
                sorted[i].height_m_ >= h_target)
            {
               double frac =
                  (h_target - sorted[i - 1].height_m_) /
                  (sorted[i].height_m_ - sorted[i - 1].height_m_ + 1e-10);
               double u_lower =
                  sorted[i - 1].wind_speed_mps_ *
                  std::sin(sorted[i - 1].wind_direction_deg_ * M_PI / 180.0);
               double v_lower =
                  sorted[i - 1].wind_speed_mps_ *
                  std::cos(sorted[i - 1].wind_direction_deg_ * M_PI / 180.0);
               double u_upper =
                  sorted[i].wind_speed_mps_ *
                  std::sin(sorted[i].wind_direction_deg_ * M_PI / 180.0);
               double v_upper =
                  sorted[i].wind_speed_mps_ *
                  std::cos(sorted[i].wind_direction_deg_ * M_PI / 180.0);
               return {u_lower + frac * (u_upper - u_lower),
                       v_lower + frac * (v_upper - v_lower)};
            }
         }
         return {0.0, 0.0};
      };

      auto [u_lower, v_lower] = getWind(lower_m);
      auto [u_upper, v_upper] = getWind(upper_m);

      return std::sqrt(std::pow(u_upper - u_lower, 2) +
                       std::pow(v_upper - v_lower, 2));
   }

   // Storm-relative helicity (m^2/s^2)
   double Srh(double lower_m, double upper_m) const
   {
      if (levels_.size() < 2)
         return 0.0;

      auto sorted = levels_;
      std::sort(sorted.begin(),
                sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      // Storm motion estimate (Bunkers Right-Mover proxy)
      // For simplicity in this implementation, using 75% of 0-6km mean wind
      // rotated 30 deg right
      double meanU = 0, meanV = 0, count = 0;
      for (const auto& lvl : sorted)
      {
         if (lvl.height_m_ >= 0 && lvl.height_m_ <= 6000.0)
         {
            meanU += lvl.wind_speed_mps_ *
                     std::sin(lvl.wind_direction_deg_ * M_PI / 180.0);
            meanV += lvl.wind_speed_mps_ *
                     std::cos(lvl.wind_direction_deg_ * M_PI / 180.0);
            count++;
         }
      }
      if (count > 0)
      {
         meanU /= count;
         meanV /= count;
      }
      double smU = meanU * 0.75; // simple proxy
      double smV = meanV * 0.75;

      double srh = 0.0;
      for (size_t i = 1; i < sorted.size(); ++i)
      {
         double h_lower = sorted[i - 1].height_m_;
         double h_upper = sorted[i].height_m_;

         if (h_lower > upper_m || h_upper < lower_m)
            continue;

         double u1 = sorted[i - 1].wind_speed_mps_ *
                     std::sin(sorted[i - 1].wind_direction_deg_ * M_PI / 180.0);
         double v1 = sorted[i - 1].wind_speed_mps_ *
                     std::cos(sorted[i - 1].wind_direction_deg_ * M_PI / 180.0);
         double u2 = sorted[i].wind_speed_mps_ *
                     std::sin(sorted[i].wind_direction_deg_ * M_PI / 180.0);
         double v2 = sorted[i].wind_speed_mps_ *
                     std::cos(sorted[i].wind_direction_deg_ * M_PI / 180.0);

         srh += (u1 - smU) * (v2 - v1) - (v1 - smV) * (u2 - u1);
      }
      return srh;
   }

   double LapseRate(double lower_m, double upper_m) const
   {
      if (levels_.size() < 2)
         return 0.0;

      auto sorted = levels_;
      std::sort(sorted.begin(),
                sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      auto getT = [&](double h_target) -> double
      {
         for (size_t i = 1; i < sorted.size(); ++i)
         {
            if (sorted[i - 1].height_m_ <= h_target &&
                sorted[i].height_m_ >= h_target)
            {
               double frac =
                  (h_target - sorted[i - 1].height_m_) /
                  (sorted[i].height_m_ - sorted[i - 1].height_m_ + 1e-10);
               return sorted[i - 1].temperature_C_ +
                      frac * (sorted[i].temperature_C_ -
                              sorted[i - 1].temperature_C_);
            }
         }
         return 0.0;
      };

      double t_lower = getT(lower_m);
      double t_upper = getT(upper_m);
      return (t_lower - t_upper) / ((upper_m - lower_m) / 1000.0);
   }

   double PrecipitableWater() const
   {
      if (levels_.size() < 2)
         return 0.0;

      auto sorted = levels_;
      std::sort(sorted.begin(),
                sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      double pwat_kg_m2 = 0.0;
      for (size_t i = 1; i < sorted.size(); ++i)
      {
         double p_lower = sorted[i - 1].pressure_hPa_;
         double p_upper = sorted[i].pressure_hPa_;
         double td_mid =
            0.5 * (sorted[i - 1].dewpoint_C_ + sorted[i].dewpoint_C_);

         double e = Es(td_mid);
         double w = kEpsilon * e / (0.5 * (p_lower + p_upper) - e);

         // PW = 1/g * integral(w dp)
         pwat_kg_m2 += (w / 9.81) * (p_lower - p_upper) *
                       100.0; // 100 to convert hPa to Pa
      }
      return pwat_kg_m2; // 1 kg/m^2 = 1 mm
   }

   void ComputeHelicity()
   {
      helicityCalculated_ = true;
      helicity0_3km_      = Srh(0, 3000);
      shear0_6km_         = BulkShear(0, 6000);
   }

   double                                latitude_ {};
   double                                longitude_ {};
   std::string                           station_id_ {};
   std::chrono::system_clock::time_point forecast_time_ {};
   std::vector<SoundingLevel>            levels_ {};
   std::vector<SoundingLevel>            parcel_profile_ {};

   // Derived parameters (lazy-computed)
   bool                  capeCalculated_ {false};
   bool                  helicityCalculated_ {false};
   double                sbcapeJkg_ {};
   double                sbcinJkg_ {};
   double                mlcapeJkg_ {};
   double                mlcinJkg_ {};
   double                mucapeJkg_ {};
   double                mucinJkg_ {};
   std::optional<double> lclPressure_ {};
   std::optional<double> lclTemperature_ {};
   std::optional<double> lfcPressure_ {};
   std::optional<double> lfcTemperature_ {};
   std::optional<double> elPressure_ {};
   std::optional<double> elTemperature_ {};
   double                helicity0_3km_ {};
   double                shear0_6km_ {};
};

SoundingData::SoundingData() : p(std::make_unique<Impl>()) {}
SoundingData::~SoundingData() = default;

SoundingData::SoundingData(SoundingData&&) noexcept            = default;
SoundingData& SoundingData::operator=(SoundingData&&) noexcept = default;

double SoundingData::latitude() const
{
   return p->latitude_;
}
double SoundingData::longitude() const
{
   return p->longitude_;
}
const std::string& SoundingData::station_id() const
{
   return p->station_id_;
}
auto SoundingData::forecast_time() const
   -> std::chrono::system_clock::time_point
{
   return p->forecast_time_;
}
const std::vector<SoundingLevel>& SoundingData::levels() const
{
   return p->levels_;
}

const std::vector<SoundingLevel>& SoundingData::parcel_profile() const
{
   if (!p->capeCalculated_)
      const_cast<SoundingData*>(this)->compute_derived();
   return p->parcel_profile_;
}

void SoundingData::set_latitude(double lat)
{
   p->latitude_ = lat;
}
void SoundingData::set_longitude(double lon)
{
   p->longitude_ = lon;
}
void SoundingData::set_station_id(const std::string& id)
{
   p->station_id_ = id;
}
void SoundingData::set_forecast_time(std::chrono::system_clock::time_point t)
{
   p->forecast_time_ = t;
}
void SoundingData::set_levels(const std::vector<SoundingLevel>& levels)
{
   p->levels_             = levels;
   p->capeCalculated_     = false;
   p->helicityCalculated_ = false;
}
void SoundingData::add_level(const SoundingLevel& level)
{
   p->levels_.push_back(level);
   p->capeCalculated_     = false;
   p->helicityCalculated_ = false;
}
double SoundingData::sbcape_jkg() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->sbcapeJkg_;
}
double SoundingData::sbcin_jkg() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->sbcinJkg_;
}
double SoundingData::mlcape_jkg() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->mlcapeJkg_;
}
double SoundingData::mlcin_jkg() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->mlcinJkg_;
}
double SoundingData::mucape_jkg() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->mucapeJkg_;
}
double SoundingData::mucin_jkg() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->mucinJkg_;
}

double SoundingData::lcl_pressure_hPa() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->lclPressure_.value_or(0.0);
}
double SoundingData::lcl_temperature_C() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->lclTemperature_.value_or(0.0);
}
double SoundingData::lfc_pressure_hPa() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->lfcPressure_.value_or(0.0);
}
double SoundingData::lfc_temperature_C() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->lfcTemperature_.value_or(0.0);
}
double SoundingData::el_pressure_hPa() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->elPressure_.value_or(0.0);
}
double SoundingData::el_temperature_C() const
{
   if (!p->capeCalculated_)
      p->ComputeCapeCin();
   return p->elTemperature_.value_or(0.0);
}

double SoundingData::bulk_shear_mps(double lower_km, double upper_km) const
{
   return p->BulkShear(lower_km * 1000.0, upper_km * 1000.0);
}
double SoundingData::storm_relative_helicity_m2s2(double lower_km,
                                                  double upper_km) const
{
   return p->Srh(lower_km * 1000.0, upper_km * 1000.0);
}

double SoundingData::lapse_rate_c_km(double lower_km, double upper_km) const
{
   return p->LapseRate(lower_km * 1000.0, upper_km * 1000.0);
}
double SoundingData::precipitable_water_mm() const
{
   return p->PrecipitableWater();
}

double SoundingData::significant_tornado_parameter() const
{
   // Simple STP (fixed layer)
   double cape  = mlcape_jkg();
   double srh   = storm_relative_helicity_m2s2(0, 1);
   double shear = bulk_shear_mps(0, 6);
   double lcl   = lcl_pressure_hPa(); // approx

   // Normalize (standard constants)
   double cape_term  = cape / 1500.0;
   double srh_term   = srh / 150.0;
   double shear_term = std::clamp(shear / 20.0, 0.0, 1.5);
   double lcl_term   = std::clamp((2000.0 - (1013.0 - lcl) * 10.0) / 1000.0,
                                0.0,
                                1.0); // very rough lcl term

   return cape_term * srh_term * shear_term * lcl_term;
}

double SoundingData::supercell_composite_parameter() const
{
   double mu_cape = mucape_jkg();
   double srh     = storm_relative_helicity_m2s2(0, 3);
   double shear   = bulk_shear_mps(0, 6);

   return (mu_cape / 1000.0) * (srh / 50.0) * (shear / 20.0);
}

double SoundingData::significant_hail_parameter() const
{
   double mu_cape      = mucape_jkg();
   double mixing_ratio = 10.0; // proxy
   double lr           = lapse_rate_c_km(7, 5);
   double t500         = -10.0; // proxy

   return (mu_cape * mixing_ratio * lr * std::abs(t500)) / 42000.0;
}

double SoundingData::energy_helicity_index() const
{
   return (mlcape_jkg() * storm_relative_helicity_m2s2(0, 2)) / 160000.0;
}

void SoundingData::compute_derived()
{
   p->ComputeCapeCin();
   p->ComputeHelicity();
}

} // namespace scwx::sounding
