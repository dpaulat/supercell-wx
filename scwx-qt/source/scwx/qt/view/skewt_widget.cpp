#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/view/skewt_widget.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <QPainter>
#include <QTimer>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx::qt::view
{

static const std::string logPrefix_ = "scwx::qt::view::skewt_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// Skew-T coordinate bounds
static constexpr double kMinTemp      = -80.0;
static constexpr double kMaxTemp      = 40.0;
static constexpr double kMinPres      = 100.0;
static constexpr double kMaxPres      = 1050.0;
static constexpr double kSkewFactor   = 40.0; // degrees C per log(p) decade

// Plot margins (pixels)
static constexpr double kMarginLeft   = 60.0;
static constexpr double kMarginRight  = 80.0;
static constexpr double kMarginTop    = 30.0;
static constexpr double kMarginBottom = 40.0;

#pragma warning(push)
#pragma warning(disable : 4458)

class SkewtWidget::Impl
{
public:
   explicit Impl(SkewtWidget* widget) :
       widget_(widget),
       shaderLoaded_(false)
   {
   }
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   // Convert skew-t coordinates to normalized device coordinates [-1, 1]
   void SkewToNDC(double tempC, double presHPa, float& nx, float& ny) const
   {
      // Skew transformation: x = temp - skew * log10(pres)
      double logP    = std::log10(presHPa);
      double skewX   = tempC - kSkewFactor * (logP - std::log10(kMaxPres));

      // Map to NDC
      double plotW = (kMaxTemp - kMinTemp) + kSkewFactor *
                    (std::log10(kMaxPres) - std::log10(kMinPres));
      double plotH = std::log10(kMaxPres) - std::log10(kMinPres);

      nx = static_cast<float>(2.0 * (skewX - kMinTemp) / plotW - 1.0);
      ny = static_cast<float>(2.0 * (std::log10(kMaxPres) - logP) / plotH - 1.0);
   }

   // Convert pressure to NDC y (no skew)
   float PresToNDCY(double presHPa) const
   {
      double plotH = std::log10(kMaxPres) - std::log10(kMinPres);
      return static_cast<float>(
          2.0 * (std::log10(kMaxPres) - std::log10(presHPa)) / plotH - 1.0);
   }

   // Temperature to NDC x at a given pressure
   float TempToNDCX(double tempC, double presHPa) const
   {
      double logP  = std::log10(presHPa);
      double skewX = tempC - kSkewFactor * (logP - std::log10(kMaxPres));
      double plotW = (kMaxTemp - kMinTemp) + kSkewFactor *
                    (std::log10(kMaxPres) - std::log10(kMinPres));
      return static_cast<float>(2.0 * (skewX - kMinTemp) / plotW - 1.0);
   }

   void DrawLine(const std::vector<float>& vertices, const std::vector<float>& colors)
   {
      if (vertices.empty()) { return; }

      GLuint vao, vbo;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> interleaved;
      interleaved.reserve(vertices.size() + colors.size());
      size_t count = vertices.size() / 3;
      for (size_t i = 0; i < count; ++i)
      {
         interleaved.push_back(vertices[i * 3]);
         interleaved.push_back(vertices[i * 3 + 1]);
         interleaved.push_back(vertices[i * 3 + 2]);
         interleaved.push_back(colors[i * 4]);
         interleaved.push_back(colors[i * 4 + 1]);
         interleaved.push_back(colors[i * 4 + 2]);
         interleaved.push_back(colors[i * 4 + 3]);
      }

      glBufferData(GL_ARRAY_BUFFER,
                   interleaved.size() * sizeof(float),
                   interleaved.data(),
                   GL_STREAM_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
      glEnableVertexAttribArray(0);

      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                            reinterpret_cast<void*>(3 * sizeof(float)));
      glEnableVertexAttribArray(1);

      glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(count));

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawFilled(const std::vector<float>& vertices,
                   const std::vector<float>& colors)
   {
      if (vertices.empty()) { return; }

      GLuint vao, vbo;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> interleaved;
      interleaved.reserve(vertices.size() + colors.size());
      size_t count = vertices.size() / 3;
      for (size_t i = 0; i < count; ++i)
      {
         interleaved.push_back(vertices[i * 3]);
         interleaved.push_back(vertices[i * 3 + 1]);
         interleaved.push_back(vertices[i * 3 + 2]);
         interleaved.push_back(colors[i * 4]);
         interleaved.push_back(colors[i * 4 + 1]);
         interleaved.push_back(colors[i * 4 + 2]);
         interleaved.push_back(colors[i * 4 + 3]);
      }

      glBufferData(GL_ARRAY_BUFFER,
                   interleaved.size() * sizeof(float),
                   interleaved.data(),
                   GL_STREAM_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                            reinterpret_cast<void*>(3 * sizeof(float)));
      glEnableVertexAttribArray(1);

      glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawPoint(float nx, float ny, float r, float g, float b, float a = 1.0f)
   {
      GLuint vao, vbo;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> ptData = {nx, ny, 0.0f, r, g, b, a};
      glBufferData(GL_ARRAY_BUFFER, ptData.size() * sizeof(float),
                   ptData.data(), GL_STREAM_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                            reinterpret_cast<void*>(3 * sizeof(float)));
      glEnableVertexAttribArray(1);

      glPointSize(8.0f);
      glDrawArrays(GL_POINTS, 0, 1);

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   // Precompute dry adiabats (θ = const, Poisson's equation)
   std::vector<std::vector<std::pair<double, double>>>
   ComputeDryAdiabats() const
   {
      // Potential temperatures: -20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80 °C
      std::vector<double> thetas;
      for (double t = -20.0; t <= 80.0; t += 10.0)
         thetas.push_back(t);

      std::vector<std::vector<std::pair<double, double>>> adiabats;
      for (double th : thetas)
      {
         double thK = th + 273.15;
         std::vector<std::pair<double, double>> curve;
         for (double p = kMaxPres; p >= kMinPres; p -= 10.0)
         {
            double t = thK * std::pow(p / 1000.0, 0.2854) - 273.15;
            if (t >= kMinTemp && t <= kMaxTemp)
            {
               curve.emplace_back(t, p);
            }
         }
         adiabats.push_back(curve);
      }
      return adiabats;
   }

   // Precompute moist adiabats (pseudo-adiabatic, θe approx)
   std::vector<std::vector<std::pair<double, double>>>
   ComputeMoistAdiabats() const
   {
      std::vector<std::vector<std::pair<double, double>>> adiabats;
      // Equivalent potential temperature at 1000 hPa: 0, 5, 10, ..., 40 °C
      for (double te = 0.0; te <= 40.0; te += 5.0)
      {
         std::vector<std::pair<double, double>> curve;
         double tCur = te;
         for (double p = kMaxPres; p >= kMinPres; p -= 10.0)
         {
            // Dry adiabat from previous level as first guess
            double pPrev = std::min(p + 10.0, kMaxPres);
            double tDry;
            if (p >= 1000.0)
               tDry = te;
            else
               tDry = (tCur + 273.15) * std::pow(p / pPrev, 0.2854) - 273.15;

            // Saturation mixing ratio at this level
            double es  = 6.112 * std::exp(17.67 * tDry / (tDry + 243.5));
            double ws  = 0.622 * es / (p - es);

            // Moist adjustment: latent heat release
            double dT = 0.0;
            if (ws > 1e-6)
            {
               // Approximate latent heating: ~2.5 °C per g/kg condensed
               double prevWs = 0.622 * es / (pPrev - es);
               double dWs = std::max(0.0, prevWs - ws);
               dT = dWs * 2500.0 * 0.001; // ~2.5°C per g/kg
            }
            tCur = tDry + dT;

            if (tCur >= kMinTemp && tCur <= kMaxTemp)
            {
               curve.emplace_back(tCur, p);
            }
         }
         adiabats.push_back(curve);
      }
      return adiabats;
   }

   // Mixing ratio lines (constant w, dashed)
   std::vector<std::vector<std::pair<double, double>>>
   ComputeMixingLines() const
   {
      std::vector<std::vector<std::pair<double, double>>> lines;
      std::vector<double> mixRatios = {0.1, 0.2, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 24.0};

      for (double w : mixRatios)
      {
         std::vector<std::pair<double, double>> curve;
         for (double p = kMaxPres; p >= kMinPres; p -= 10.0)
         {
            double e  = w * p / (0.622 + w);
            double td = 243.5 * std::log(e / 6.112) /
                        (17.67 - std::log(e / 6.112));
            if (td >= kMinTemp && td <= kMaxTemp)
            {
               curve.emplace_back(td, p);
            }
         }
         lines.push_back(curve);
      }
      return lines;
   }

   // Isotherm lines (constant T, skewed)
   std::vector<std::vector<std::pair<double, double>>>
   ComputeIsotherms() const
   {
      std::vector<std::vector<std::pair<double, double>>> isotherms;
      for (double t = -80.0; t <= 40.0; t += 10.0)
      {
         std::vector<std::pair<double, double>> line;
         for (double p = kMaxPres; p >= kMinPres; p -= 10.0)
         {
            if (t >= kMinTemp && t <= kMaxTemp)
            {
               line.emplace_back(t, p);
            }
         }
         isotherms.push_back(line);
      }
      return isotherms;
   }

   void DrawGrid()
   {
      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      // Draw isotherms (gray solid, skewed)
      auto isotherms = ComputeIsotherms();
      for (const auto& iso : isotherms)
      {
         if (iso.size() < 2) { continue; }
         std::vector<float> verts, cols;
         for (const auto& pt : iso)
         {
            float nx, ny;
            SkewToNDC(pt.first, pt.second, nx, ny);
            verts.insert(verts.end(), {nx, ny, 0.0f});
            cols.insert(cols.end(), {0.35f, 0.35f, 0.5f, 0.7f});
         }
         DrawLine(verts, cols);
      }

      // Draw isobars (gray solid horizontal, log-spaced)
      for (double p = 100.0; p <= 1050.0; p += 100.0)
      {
         float ny = PresToNDCY(p);
         float nx1 = TempToNDCX(kMinTemp, p);
         float nx2 = TempToNDCX(kMaxTemp, p);

         std::vector<float> verts = {nx1, ny, 0.0f, nx2, ny, 0.0f};
         std::vector<float> cols  = {0.35f, 0.35f, 0.5f, 0.7f,
                                     0.35f, 0.35f, 0.5f, 0.7f};
         DrawLine(verts, cols);
      }
   }

   void DrawReferenceLines()
   {
      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      // Dry adiabats (solid orange)
      auto dry = ComputeDryAdiabats();
      for (const auto& curve : dry)
      {
         if (curve.size() < 2) { continue; }
         std::vector<float> verts, cols;
         for (const auto& pt : curve)
         {
            float nx, ny;
            SkewToNDC(pt.first, pt.second, nx, ny);
            verts.insert(verts.end(), {nx, ny, 0.0f});
            cols.insert(cols.end(), {1.0f, 0.7f, 0.2f, 0.4f});
         }
         DrawLine(verts, cols);
      }

      // Moist adiabats (solid green)
      auto moist = ComputeMoistAdiabats();
      for (const auto& curve : moist)
      {
         if (curve.size() < 2) { continue; }
         std::vector<float> verts, cols;
         for (const auto& pt : curve)
         {
            float nx, ny;
            SkewToNDC(pt.first, pt.second, nx, ny);
            verts.insert(verts.end(), {nx, ny, 0.0f});
            cols.insert(cols.end(), {0.0f, 0.8f, 0.4f, 0.4f});
         }
         DrawLine(verts, cols);
      }

      // Mixing ratio lines (dashed blue)
      auto mix = ComputeMixingLines();
      for (const auto& curve : mix)
      {
         if (curve.size() < 2) { continue; }
         std::vector<float> verts, cols;
         for (const auto& pt : curve)
         {
            float nx, ny;
            SkewToNDC(pt.first, pt.second, nx, ny);
            verts.insert(verts.end(), {nx, ny, 0.0f});
            cols.insert(cols.end(), {0.3f, 0.6f, 1.0f, 0.5f});
         }
         DrawLine(verts, cols);
      }
   }

   void DrawProfiles()
   {
      if (!sounding_ || sounding_->levels().empty()) { return; }

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      // Sort by decreasing pressure
      auto levels = sounding_->levels();
      std::sort(levels.begin(), levels.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      // Temperature profile (red)
      {
         std::vector<float> verts, cols;
         for (const auto& lvl : levels)
         {
            float nx, ny;
            SkewToNDC(lvl.temperature_C_, lvl.pressure_hPa_, nx, ny);
            verts.insert(verts.end(), {nx, ny, 0.0f});
            cols.insert(cols.end(), {1.0f, 0.2f, 0.2f, 1.0f});
         }
         DrawLine(verts, cols);
      }

      // Dewpoint profile (green)
      {
         std::vector<float> verts, cols;
         for (const auto& lvl : levels)
         {
            if (lvl.dewpoint_C_ < -99.0) { continue; } // skip invalid
            float nx, ny;
            SkewToNDC(lvl.dewpoint_C_, lvl.pressure_hPa_, nx, ny);
            verts.insert(verts.end(), {nx, ny, 0.0f});
            cols.insert(cols.end(), {0.2f, 1.0f, 0.2f, 1.0f});
         }
         DrawLine(verts, cols);
      }
   }

   void DrawParcelProfile()
   {
      if (!sounding_ || sounding_->levels().empty()) { return; }

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      auto levels = sounding_->levels();
      std::sort(levels.begin(), levels.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      const auto& sfc     = levels[0];
      double pSfc         = sfc.pressure_hPa_;
      double tSfc         = sfc.temperature_C_;
      double pLcl = sounding_->lcl_pressure_hPa();

      // Compute parcel profile
      std::vector<float> verts, cols;

      for (const auto& lvl : levels)
      {
          double p = lvl.pressure_hPa_;
          if (p > pSfc) { continue; }

          double tParcel;
          if (p >= pLcl)
          {
             tParcel = (tSfc + 273.15) * std::pow(p / pSfc, 0.2854) - 273.15;
          }
          else
          {
             double tAtLcl = (tSfc + 273.15) * std::pow(pLcl / pSfc, 0.2854) - 273.15;
             double tk = tAtLcl + 273.15;
             tk += 6.5 * (pLcl - p) / 1000.0;
             tParcel = tk - 273.15;
          }

         float nx, ny;
         SkewToNDC(tParcel, p, nx, ny);
         verts.insert(verts.end(), {nx, ny, 0.0f});
          cols.insert(cols.end(), {1.0f, 1.0f, 1.0f, 0.9f});
       }

       DrawLine(verts, cols);
    }

    void DrawCapeCin()
    {
      if (!sounding_ || sounding_->levels().empty()) { return; }

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      auto levels = sounding_->levels();
      if (levels.empty()) { return; }

      std::sort(levels.begin(), levels.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      const auto& sfc  = levels[0];
      double pSfc      = sfc.pressure_hPa_;
      double tSfc      = sfc.temperature_C_;
      double pLcl      = sounding_->lcl_pressure_hPa();

      // Build triangles between temperature profile and parcel profile
      std::vector<float> capeVerts, capeColors;
      std::vector<float> cinVerts, cinColors;

      for (size_t i = 1; i < levels.size(); ++i)
      {
         double pUpper = levels[i].pressure_hPa_;
         double pLower = levels[i - 1].pressure_hPa_;
         double tEnvUpper = levels[i].temperature_C_;
         double tEnvLower = levels[i - 1].temperature_C_;

         if (pUpper > pSfc) { continue; }

         // Compute parcel temperature at lower and upper level
         auto parcelT = [&](double presP) -> double
         {
            if (presP >= pLcl)
               return (tSfc + 273.15) * std::pow(presP / pSfc, 0.2854) - 273.15;
            double tAtLcl = (tSfc + 273.15) * std::pow(pLcl / pSfc, 0.2854) - 273.15;
            return (tAtLcl + 273.15) + 6.5 * (pLcl - presP) / 1000.0 - 273.15;
         };

         double tParcelLower = parcelT(pLower);
         double tParcelUpper = parcelT(pUpper);

         if (tParcelLower < -100 || tParcelUpper < -100) { continue; }

         float nx1, ny1, nx2, ny2, nx3, ny3, nx4, ny4;
         SkewToNDC(tEnvLower, pLower, nx1, ny1);
         SkewToNDC(tEnvUpper, pUpper, nx2, ny2);
         SkewToNDC(tParcelLower, pLower, nx3, ny3);
         SkewToNDC(tParcelUpper, pUpper, nx4, ny4);

         // Determine if this layer is CAPE or CIN
         double tEnvMid = 0.5 * (tEnvLower + tEnvUpper);
         double tParcelMid = 0.5 * (tParcelLower + tParcelUpper);

         auto& targetVerts = (tParcelMid > tEnvMid) ? capeVerts : cinVerts;
         auto& targetColors = (tParcelMid > tEnvMid) ? capeColors : cinColors;

         // Triangle 1: lower-left, upper-left, upper-right
         targetVerts.insert(targetVerts.end(), {nx1, ny1, 0.0f, nx2, ny2, 0.0f,
                                                 nx4, ny4, 0.0f});
         // Triangle 2: lower-left, upper-right, lower-right
         targetVerts.insert(targetVerts.end(), {nx1, ny1, 0.0f, nx4, ny4, 0.0f,
                                                 nx3, ny3, 0.0f});

         // Colors
         float cr, cg, cb;
          if (tParcelMid > tEnvMid)
          {
             cr = 1.0f; cg = 0.2f; cb = 0.2f; // CAPE red
          }
          else
          {
             cr = 0.3f; cg = 0.3f; cb = 1.0f; // CIN blue
          }

          for (int t = 0; t < 6; ++t)
          {
             targetColors.insert(targetColors.end(), {cr, cg, cb, 0.25f});
         }
      }

      if (!capeVerts.empty()) { DrawFilled(capeVerts, capeColors); }
      if (!cinVerts.empty()) { DrawFilled(cinVerts, cinColors); }
   }

   void DrawMarkers()
   {
      if (!sounding_) { return; }

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      // LCL marker (cyan dot)
      {
         double pLcl = sounding_->lcl_pressure_hPa();
         double tLcl = sounding_->lcl_temperature_C();
         if (pLcl > 0)
         {
            float nx, ny;
            SkewToNDC(tLcl, pLcl, nx, ny);
            DrawPoint(nx, ny, 0.0f, 1.0f, 1.0f);
         }
      }

      // LFC marker (yellow dot)
      {
         double pLfc = sounding_->lfc_pressure_hPa();
         double tLfc = sounding_->lfc_temperature_C();
         if (pLfc > 0)
         {
            float nx, ny;
            SkewToNDC(tLfc, pLfc, nx, ny);
            DrawPoint(nx, ny, 1.0f, 1.0f, 0.0f);
         }
      }

      // EL marker (magenta dot)
      {
         double pEl = sounding_->el_pressure_hPa();
         double tEl = sounding_->el_temperature_C();
         if (pEl > 0)
         {
            float nx, ny;
            SkewToNDC(tEl, pEl, nx, ny);
            DrawPoint(nx, ny, 1.0f, 0.0f, 1.0f);
         }
      }
   }

   void DrawWindBarbs()
   {
      if (!sounding_) { return; }

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      auto levels = sounding_->levels();
      std::sort(levels.begin(), levels.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      // Draw wind barbs on the right side of the plot
      // Barb position: fixed x on the right, y at each pressure level spacing
      float barbNX = TempToNDCX(kMaxTemp, kMaxPres) - 0.02f;

      for (const auto& lvl : levels)
      {
         float ny = PresToNDCY(lvl.pressure_hPa_);

         double dirRad = lvl.wind_direction_deg_ * M_PI / 180.0;
         double scale  = lvl.wind_speed_mps_ / 50.0; // normalized barb length

         // Barb shaft (vertical line at barb position)
         float shaftTop = ny + 0.01f;
         float shaftBot = ny - 0.01f;
         std::vector<float> shaftVerts = {barbNX, shaftBot, 0.0f,
                                           barbNX, shaftTop, 0.0f};
          std::vector<float> shaftCols  = {0.7f, 0.7f, 0.8f, 1.0f,
                                           0.7f, 0.7f, 0.8f, 1.0f};
          DrawLine(shaftVerts, shaftCols);

          if (lvl.wind_speed_mps_ < 1.0) { continue; }

          // Barb flag (line from shaft in wind direction)
          float flagLen = 0.025f * std::min(scale, 1.0);
          float dx      = flagLen * std::sin(dirRad);
          float dy      = flagLen * std::cos(dirRad);

          std::vector<float> flagVerts = {barbNX, ny, 0.0f,
                                           barbNX + dx, ny + dy, 0.0f};
          std::vector<float> flagCols  = {0.7f, 0.7f, 0.8f, 1.0f,
                                           0.7f, 0.7f, 0.8f, 1.0f};
         DrawLine(flagVerts, flagCols);
      }
   }

   void DrawAxisLabels(QPainter& painter)
   {
      painter.setRenderHint(QPainter::Antialiasing);

      // Title
      if (sounding_)
      {
         std::string id = sounding_->station_id();
         painter.setPen(QColor(200, 200, 220));
         QFont titleFont = painter.font();
         titleFont.setPointSize(11);
         titleFont.setBold(true);
         painter.setFont(titleFont);
         painter.drawText(rect_, Qt::AlignTop | Qt::AlignHCenter,
                          QString::fromStdString("GFS Sounding: " + id));
      }

      // Temperature labels along bottom (x-axis, skewed)
      painter.setPen(QColor(180, 180, 200));
      QFont labelFont = painter.font();
      labelFont.setPointSize(8);
      painter.setFont(labelFont);

      for (double t = -80.0; t <= 40.0; t += 10.0)
      {
         float nx, ny;
         SkewToNDC(t, kMaxPres, nx, ny);
         double wx = (nx + 1.0) / 2.0 * rect_.width();
         painter.drawText(QPointF(wx - 10, rect_.height() - 10),
                         QString::number(static_cast<int>(t)) + "\u00B0");
      }

      // Pressure labels along left (y-axis, log scale)
      double presLabels[] = {1050.0, 1000.0, 900.0, 800.0, 700.0, 600.0,
                             500.0, 400.0, 300.0, 200.0, 100.0, 50.0};
      for (double p : presLabels)
      {
          if (p < kMinPres || p > kMaxPres) { continue; }
          float ny = PresToNDCY(p);
          double wy = (1.0 - ny) / 2.0 * rect_.height();
          painter.drawText(QPointF(5, wy + 3),
                          QString::number(static_cast<int>(p)));
      }

      // CAPE/CIN values
      if (sounding_)
      {
         painter.setPen(QColor(255, 100, 100));
         painter.drawText(QPointF(rect_.width() - 150, 20),
                          QString("CAPE: %1 J/kg")
                              .arg(static_cast<int>(sounding_->cape_jkg())));

         painter.setPen(QColor(100, 150, 255));
         painter.drawText(QPointF(rect_.width() - 150, 35),
                          QString("CIN: %1 J/kg")
                              .arg(static_cast<int>(sounding_->cin_jkg())));
      }
   }

   void Render()
   {
      if (!shaderLoaded_) { return; }

      glClearColor(0.08f, 0.08f, 0.15f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      // Orthographic projection for NDC space
      projMatrix_ = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

      // Enable blending for transparency
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      DrawGrid();
      DrawReferenceLines();
      DrawCapeCin();
      DrawProfiles();
      DrawParcelProfile();
      DrawMarkers();
      DrawWindBarbs();

      glDisable(GL_BLEND);
   }

   SkewtWidget*                                  widget_;
   std::shared_ptr<gl::ShaderProgram>            shader_;
   glm::mat4                                     projMatrix_ {};
   std::shared_ptr<sounding::SoundingData>       sounding_;
   QRect                                         rect_ {};
   bool                                          shaderLoaded_ {false};
};

SkewtWidget::SkewtWidget(QWidget* parent) :
    QOpenGLWidget(parent),
    p(std::make_unique<Impl>(this))
{
   setMinimumSize(400, 500);
}
SkewtWidget::~SkewtWidget() = default;

void SkewtWidget::SetSounding(std::shared_ptr<sounding::SoundingData> sounding)
{
   p->sounding_ = sounding;
   update();
}

void SkewtWidget::initializeGL()
{
   p->shader_ = std::make_shared<gl::ShaderProgram>();
   if (!p->shader_->Load(":/gl/color.vert", ":/gl/color.frag"))
   {
      logger_->error("Failed to load skew-t shaders");
      return;
   }

   p->shaderLoaded_ = true;
   logger_->debug("SkewtWidget OpenGL initialized");
}

void SkewtWidget::resizeGL(int w, int h)
{
   p->rect_ = QRect(0, 0, w, h);
   glViewport(0, 0, w, h);
}

void SkewtWidget::paintGL()
{
   p->Render();

   // Overlay text via QPainter
   QPainter painter(this);
   p->DrawAxisLabels(painter);
   painter.end();
}

} // namespace scwx::qt::view
