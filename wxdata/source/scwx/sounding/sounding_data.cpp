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
static constexpr double kRd       = 287.058;   // J/(kg·K) - dry air gas constant
static constexpr double kCp       = 1005.0;     // J/(kg·K) - specific heat of dry air at constant pressure
static constexpr double kRv       = 461.5;      // J/(kg·K) - water vapor gas constant
static constexpr double kLv       = 2.501e6;    // J/kg - latent heat of vaporization at 0°C
static constexpr double kGamma    = kRd / kCp;  // 0.2854...
static constexpr double kEpsilon  = kRd / kRv;  // 0.622...

class SoundingData::Impl
{
public:
   explicit Impl()  = default;
   ~Impl()          = default;

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
      return 1000.0 * kEpsilon * e / 100.0; // simplified: w = ε*e/p, using p=1000hPa approx
   }

   // LCL temperature using Bolton's formula
   static double LclTemperature(double t_C, double td_C)
   {
      double tk = t_C + 273.15;
      double tdk = td_C + 273.15;
      // Bolton (1980): 1/T_LCL = 1/(T_d-56) + ln(T/T_d)/800
      double inv_tlcl = 1.0 / (tdk - 56.0) + std::log(tk / tdk) / 800.0;
      return (1.0 / inv_tlcl) - 273.15;
   }

   // LCL pressure using the dry adiabat
   static double LclPressure(double p_sfc, double t_C, double td_C)
   {
      double tk    = t_C + 273.15;
      double tlcl  = LclTemperature(t_C, td_C) + 273.15;
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

   // Moist adiabatic temperature at new pressure (pseudo-adiabatic approximation)
   // Uses iterative method
   static double MoistAdiabatT(double t_start_C, double p_start, double p_end)
   {
      if (p_end >= p_start) { return t_start_C; }

      double tk = t_start_C + 273.15;
      double p  = p_start;
      double dp = (p_end - p_start) / 50.0; // 50 steps for integration
      if (dp >= 0.0) { dp = -1.0; }

      for (int step = 0; step < 50 && ((dp < 0 && p > p_end) || (dp > 0 && p < p_end)); ++step)
      {
         double es   = Es(tk - 273.15);
         double ws   = kEpsilon * es / (p - es);
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

   // CAPE calculation
   void ComputeCapeCin()
   {
      capeCalculated_ = true;
      capeJkg_        = 0.0;
      cinJkg_         = 0.0;

      if (levels_.size() < 2)
      {
         lclPressure_  = std::nullopt;
         lclTemperature_ = std::nullopt;
         return;
      }

      // Surface level for parcel
      const auto& sfc = levels_[0];
      double p_sfc   = sfc.pressure_hPa_;
      double t_sfc   = sfc.temperature_C_;
      double td_sfc  = sfc.dewpoint_C_;

      // Compute LCL
      lclPressure_  = LclPressure(p_sfc, t_sfc, td_sfc);
      lclTemperature_ = LclTemperature(t_sfc, td_sfc);

      // Sort levels by decreasing pressure (surface up)
      auto sorted = levels_;
      std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      // Parcel profile and CAPE/CIN integration
      bool aboveLcl     = false;
      bool crossedLfc   = false;
      double capeInt    = 0.0;
      double cinInt     = 0.0;

      for (std::size_t i = 1; i < sorted.size(); ++i)
      {
         double p_upper = sorted[i].pressure_hPa_;
         double p_lower = sorted[i - 1].pressure_hPa_;
         double t_env   = 0.5 * (sorted[i].temperature_C_ +
                                 sorted[i - 1].temperature_C_);
         double p_mid   = 0.5 * (p_upper + p_lower);

         // Parcel temperature at this level
         double t_parcel;
         if (!aboveLcl && p_mid > *lclPressure_)
         {
            t_parcel = DryAdiabatT(t_sfc, p_sfc, p_mid);
         }
         else
         {
            if (!aboveLcl)
            {
               aboveLcl = true;
            }
            double t_at_lcl = DryAdiabatT(t_sfc, p_sfc, *lclPressure_);
            t_parcel = MoistAdiabatT(t_at_lcl, *lclPressure_, p_mid);
         }

         // Integration element: d(CAPE) = R_d * (T_vp - T_ve) * d(ln p)
         double tv_parcel = VirtualTemperature(t_parcel, 0.0); // approx
         double tv_env    = VirtualTemperature(t_env, 0.0);
         double dlnp      = std::log(p_lower / p_upper);
         double integrand = kRd * (tv_parcel - tv_env) * dlnp;

         if (integrand > 0)
         {
            capeInt += integrand;
            if (!crossedLfc)
            {
               crossedLfc = true;
               lfcPressure_  = p_mid;
               lfcTemperature_ = t_env;
            }
         }
         else
         {
            cinInt += integrand;
         }
      }

      capeJkg_ = capeInt;
      cinJkg_  = std::abs(cinInt);

      // Compute EL (Equilibrium Level) - where parcel profile crosses T_env
      if (crossedLfc)
      {
         for (std::size_t i = 1; i < sorted.size(); ++i)
         {
            double p_upper = sorted[i].pressure_hPa_;
            double p_lower = sorted[i - 1].pressure_hPa_;
            double t_env_upper = sorted[i].temperature_C_;
            double t_env_lower = sorted[i - 1].temperature_C_;

            double t_parcel_upper, t_parcel_lower;
            if (p_upper > *lclPressure_)
            {
               t_parcel_upper = DryAdiabatT(t_sfc, p_sfc, p_upper);
               t_parcel_lower = DryAdiabatT(t_sfc, p_sfc, p_lower);
            }
            else
            {
               double t_at_lcl = DryAdiabatT(t_sfc, p_sfc, *lclPressure_);
               t_parcel_upper = MoistAdiabatT(t_at_lcl, *lclPressure_, p_upper);
               t_parcel_lower = MoistAdiabatT(t_at_lcl, *lclPressure_, p_lower);
            }

            // Check for crossing
            if ((t_parcel_lower - t_env_lower) >= 0 &&
                (t_parcel_upper - t_env_upper) <= 0)
            {
               // Linear interpolation to find crossing pressure
               double frac = (t_parcel_lower - t_env_lower) /
                             ((t_parcel_lower - t_env_lower) -
                              (t_parcel_upper - t_env_upper) + 1e-10);
               elPressure_     = p_lower + frac * (p_upper - p_lower);
               elTemperature_  = t_env_lower + frac * (t_env_upper - t_env_lower);
               break;
            }
         }
      }

      if (!elPressure_.has_value())
      {
         elPressure_     = sorted.back().pressure_hPa_;
         elTemperature_  = sorted.back().temperature_C_;
      }
   }

   // Storm-relative helicity
   void ComputeHelicity()
   {
      helicityCalculated_ = true;
      helicityM2s2_       = 0.0;
      shear_1to6km_       = 0.0;

      if (levels_.size() < 2) { return; }

      // Sort by decreasing pressure (increasing height)
      auto sorted = levels_;
      std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      // Find 0-3km layer for helicity
      // Estimate height using barometric formula if not available
      auto heightAtPressure = [&](double p_target) -> double
      {
         for (std::size_t i = 1; i < sorted.size(); ++i)
         {
            if (sorted[i].pressure_hPa_ <= p_target &&
                sorted[i - 1].pressure_hPa_ >= p_target)
            {
               double frac = (sorted[i - 1].pressure_hPa_ - p_target) /
                             (sorted[i - 1].pressure_hPa_ - sorted[i].pressure_hPa_);
               return sorted[i - 1].height_m_ +
                      frac * (sorted[i].height_m_ - sorted[i - 1].height_m_);
            }
         }
         return -1.0;
      };

      // SRH 0-3km: sum of (V - c) × dl over the layer
      // Using storm motion estimate: 75% of mean 0-6km wind, 30 deg right
      // For simplicity, using Bunkers storm motion
      double meanU = 0.0, meanV = 0.0;
      int count    = 0;
      for (const auto& lvl : sorted)
      {
         if (lvl.height_m_ >= 0 && lvl.height_m_ <= 6000.0)
         {
            double u = lvl.wind_speed_mps_ *
                       std::sin(lvl.wind_direction_deg_ * M_PI / 180.0);
            double v = lvl.wind_speed_mps_ *
                       std::cos(lvl.wind_direction_deg_ * M_PI / 180.0);
            meanU += u;
            meanV += v;
            count++;
         }
      }
      if (count > 0)
      {
         meanU /= count;
         meanV /= count;
      }

      // SRH using the effective storm motion approximation
      double srh = 0.0;
      for (std::size_t i = 1; i < sorted.size(); ++i)
      {
         if (sorted[i].height_m_ > 3000.0 || sorted[i - 1].height_m_ < 0)
         {
            continue;
         }
         double u_i   = sorted[i].wind_speed_mps_ *
                        std::sin(sorted[i].wind_direction_deg_ * M_PI / 180.0);
         double v_i   = sorted[i].wind_speed_mps_ *
                        std::cos(sorted[i].wind_direction_deg_ * M_PI / 180.0);
         double u_im1 = sorted[i - 1].wind_speed_mps_ *
                        std::sin(sorted[i - 1].wind_direction_deg_ * M_PI / 180.0);
         double v_im1 = sorted[i - 1].wind_speed_mps_ *
                        std::cos(sorted[i - 1].wind_direction_deg_ * M_PI / 180.0);

         double du = u_i - u_im1;
         double dv = v_i - v_im1;

         // SRH = sum of (u_i * dv_i - v_i * du_i) ... actually
         // SRH = sum over layers of (V_layer - c) × Δh
         // Simplified: srh += (u_i - c_u) * dv - (v_i - c_v) * du
         // For Bunkers motion estimate, simply use mean wind as proxy
         srh += (u_i - meanU) * dv - (v_i - meanV) * du;
      }

      // 1-6km shear magnitude
      double h1 = -1, h6 = -1;
      double u1 = 0, v1 = 0, u6 = 0, v6 = 0;

      for (std::size_t i = 0; i + 1 < sorted.size(); ++i)
      {
         double h_lower = sorted[i].height_m_;
         double h_upper = sorted[i + 1].height_m_;

         if (h1 < 0 && h_lower <= 1000.0 && h_upper >= 1000.0)
         {
            double frac = (1000.0 - h_lower) / (h_upper - h_lower);
            u1 = sorted[i].wind_speed_mps_ *
                    std::sin(sorted[i].wind_direction_deg_ * M_PI / 180.0) +
                 frac * (sorted[i + 1].wind_speed_mps_ *
                             std::sin(sorted[i + 1].wind_direction_deg_ * M_PI / 180.0) -
                         sorted[i].wind_speed_mps_ *
                             std::sin(sorted[i].wind_direction_deg_ * M_PI / 180.0));
            v1 = sorted[i].wind_speed_mps_ *
                    std::cos(sorted[i].wind_direction_deg_ * M_PI / 180.0) +
                 frac * (sorted[i + 1].wind_speed_mps_ *
                             std::cos(sorted[i + 1].wind_direction_deg_ * M_PI / 180.0) -
                         sorted[i].wind_speed_mps_ *
                             std::cos(sorted[i].wind_direction_deg_ * M_PI / 180.0));
            h1 = 1000.0;
         }

         if (h6 < 0 && h_lower <= 6000.0 && h_upper >= 6000.0)
         {
            double frac = (6000.0 - h_lower) / (h_upper - h_lower);
            u6 = sorted[i].wind_speed_mps_ *
                    std::sin(sorted[i].wind_direction_deg_ * M_PI / 180.0) +
                 frac * (sorted[i + 1].wind_speed_mps_ *
                             std::sin(sorted[i + 1].wind_direction_deg_ * M_PI / 180.0) -
                         sorted[i].wind_speed_mps_ *
                             std::sin(sorted[i].wind_direction_deg_ * M_PI / 180.0));
            v6 = sorted[i].wind_speed_mps_ *
                    std::cos(sorted[i].wind_direction_deg_ * M_PI / 180.0) +
                 frac * (sorted[i + 1].wind_speed_mps_ *
                             std::cos(sorted[i + 1].wind_direction_deg_ * M_PI / 180.0) -
                         sorted[i].wind_speed_mps_ *
                             std::cos(sorted[i].wind_direction_deg_ * M_PI / 180.0));
            h6 = 6000.0;
         }
      }

      if (h1 > 0 && h6 > 0)
      {
         shear_1to6km_ = std::sqrt((u6 - u1) * (u6 - u1) + (v6 - v1) * (v6 - v1)) /
                         ((h6 - h1) / 1000.0);
      }

      helicityM2s2_ = srh;
   }

   double                         latitude_ {};
   double                         longitude_ {};
   std::string                    station_id_ {};
   std::chrono::system_clock::time_point forecast_time_ {};
   std::vector<SoundingLevel>     levels_ {};

   // Derived parameters (lazy-computed)
   bool                           capeCalculated_ {false};
   bool                           helicityCalculated_ {false};
   double                         capeJkg_ {};
   double                         cinJkg_ {};
   std::optional<double>          lclPressure_ {};
   std::optional<double>          lclTemperature_ {};
   std::optional<double>          lfcPressure_ {};
   std::optional<double>          lfcTemperature_ {};
   std::optional<double>          elPressure_ {};
   std::optional<double>          elTemperature_ {};
   double                         helicityM2s2_ {};
   double                         shear_1to6km_ {};
};

SoundingData::SoundingData() : p(std::make_unique<Impl>()) {}
SoundingData::~SoundingData() = default;

SoundingData::SoundingData(SoundingData&&) noexcept            = default;
SoundingData& SoundingData::operator=(SoundingData&&) noexcept = default;

double SoundingData::latitude() const { return p->latitude_; }
double SoundingData::longitude() const { return p->longitude_; }
const std::string& SoundingData::station_id() const { return p->station_id_; }
auto SoundingData::forecast_time() const -> std::chrono::system_clock::time_point
{
   return p->forecast_time_;
}
const std::vector<SoundingLevel>& SoundingData::levels() const { return p->levels_; }

void SoundingData::set_latitude(double lat) { p->latitude_ = lat; }
void SoundingData::set_longitude(double lon) { p->longitude_ = lon; }
void SoundingData::set_station_id(const std::string& id) { p->station_id_ = id; }
void SoundingData::set_forecast_time(std::chrono::system_clock::time_point t)
{
   p->forecast_time_ = t;
}
void SoundingData::set_levels(const std::vector<SoundingLevel>& levels)
{
   p->levels_           = levels;
   p->capeCalculated_   = false;
   p->helicityCalculated_ = false;
}
void SoundingData::add_level(const SoundingLevel& level)
{
   p->levels_.push_back(level);
   p->capeCalculated_   = false;
   p->helicityCalculated_ = false;
}

double SoundingData::cape_jkg() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->capeJkg_;
}
double SoundingData::cin_jkg() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->cinJkg_;
}
double SoundingData::lcl_pressure_hPa() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->lclPressure_.value_or(0.0);
}
double SoundingData::lcl_temperature_C() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->lclTemperature_.value_or(0.0);
}
double SoundingData::lfc_pressure_hPa() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->lfcPressure_.value_or(0.0);
}
double SoundingData::lfc_temperature_C() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->lfcTemperature_.value_or(0.0);
}
double SoundingData::el_pressure_hPa() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->elPressure_.value_or(0.0);
}
double SoundingData::el_temperature_C() const
{
   if (!p->capeCalculated_) { const_cast<Impl*>(p.get())->ComputeCapeCin(); }
   return p->elTemperature_.value_or(0.0);
}

double SoundingData::surface_based_shear_s_1(double lower_km, double upper_km) const
{
   if (!p->helicityCalculated_) { const_cast<Impl*>(p.get())->ComputeHelicity(); }
   // Recalculate for specific bounds if needed
   (void)lower_km;
   (void)upper_km;
   return p->shear_1to6km_;
}
double SoundingData::storm_relative_helicity_m2s2(double lower_km,
                                                   double upper_km) const
{
   if (!p->helicityCalculated_) { const_cast<Impl*>(p.get())->ComputeHelicity(); }
   (void)lower_km;
   (void)upper_km;
   return p->helicityM2s2_;
}

void SoundingData::compute_derived()
{
   p->ComputeCapeCin();
   p->ComputeHelicity();
}

} // namespace scwx::sounding
