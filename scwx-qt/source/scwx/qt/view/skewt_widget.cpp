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
#include <string>
#include <vector>

#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

namespace scwx::qt::view
{

static const std::string logPrefix_ = "scwx::qt::view::skewt_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

// Skew-T coordinate bounds
static constexpr double kMinTemp    = -80.0;
static constexpr double kMaxTemp    = 40.0;
static constexpr double kMinPres    = 100.0;
static constexpr double kMaxPres    = 1050.0;
static constexpr double kSkewFactor = 40.0; // degrees C per log(p) decade

// Plot margins (pixels)
// Plot margins (pixels) - Removed unused constants

// Rendering constants
static constexpr float   kNDCRange       = 2.0f;
static constexpr float   kNDCOffset      = 1.0f;
static constexpr GLsizei kStride7        = 7 * sizeof(float);
static constexpr int     kTooltipOffset  = 10;
static constexpr float   kAlphaHover     = 0.5f;
static constexpr float   kBarbStemLength = 0.08f;
static constexpr float   kBarbTickLength = 0.03f;
static constexpr float   kBarbTickAngle  = 60.0f; // degrees from stem
static constexpr float   kBarbTickStep   = 0.015f;
static constexpr float   kBarbX          = 0.92f;

template<typename T>
static inline const void* BufferOffset(T offset)
{
   return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
}

#pragma warning(push)
#pragma warning(disable : 4458)

class SkewtWidget::Impl
{
public:
   explicit Impl(SkewtWidget* widget) : widget_(widget) {}
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   // Convert NDC [-1, 1] back to skew-t coordinates
   void NDCToSkewT(float nx, float ny, double& tempC, double& presHPa) const
   {
      double plotW =
         (kMaxTemp - kMinTemp) +
         kSkewFactor * (std::log10(kMaxPres) - std::log10(kMinPres));
      double plotH = std::log10(kMaxPres) - std::log10(kMinPres);

      // ny = 2.0 * (log10(kMaxPres) - logP) / plotH - 1.0
      static constexpr double kNDCScale = 2.0;
      double                  logP      = std::log10(kMaxPres) -
                    (static_cast<double>(ny) + 1.0) * plotH / kNDCScale;
      presHPa = std::pow(10.0, logP);

      // nx = 2.0 * (skewX - kMinTemp) / plotW - 1.0
      double skewX =
         (static_cast<double>(nx) + 1.0) * plotW / kNDCScale + kMinTemp;
      tempC = skewX + kSkewFactor * (logP - std::log10(kMaxPres));
   }

   void DrawHoverLine()
   {
      if (hoverPressure_ <= 0)
         return;

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1,
                         GL_FALSE,
                         glm::value_ptr(projMatrix_));

      float ny  = PresToNDCY(hoverPressure_);
      float nx1 = TempToNDCX(kMinTemp, hoverPressure_);
      float nx2 = TempToNDCX(kMaxTemp, hoverPressure_);

      std::vector<float> verts = {nx1, ny, 0.0f, nx2, ny, 0.0f};
      std::vector<float> cols  = {
         1.0f, 1.0f, 1.0f, kAlphaHover, 1.0f, 1.0f, 1.0f, kAlphaHover};
      DrawLine(verts, cols);
   }

   void DrawTooltip(QPainter& painter)
   {
      if (hoverPressure_ <= 0 || !sounding_)
         return;

      double tempC = 0.0, presHPa = 0.0;
      NDCToSkewT(hoverNDC_.x, hoverNDC_.y, tempC, presHPa);

      // Find closest sounding level
      const auto& levels = sounding_->levels();
      auto        it =
         std::min_element(levels.begin(),
                          levels.end(),
                          [&](const auto& a, const auto& b)
                          {
                             return std::abs(a.pressure_hPa_ - hoverPressure_) <
                                    std::abs(b.pressure_hPa_ - hoverPressure_);
                          });

      if (it == levels.end())
         return;

      QString text = QString("P: %1 hPa\nZ: %2 m\nT: %3°C\nTd: %4°C")
                        .arg(it->pressure_hPa_, 0, 'f', 1)
                        .arg(it->height_m_, 0, 'f', 0)
                        .arg(it->temperature_C_, 0, 'f', 1)
                        .arg(it->dewpoint_C_, 0, 'f', 1);

      painter.setPen(Qt::white);
      painter.setBackground(QBrush(QColor(0, 0, 0, 160)));
      painter.setBackgroundMode(Qt::OpaqueMode);
      static constexpr int kTooltipOffsetY = 10;
      painter.drawText(
         hoverPos_.x() + kTooltipOffset, hoverPos_.y() - kTooltipOffsetY, text);
   }

   // Convert temperature and pressure to NDC x
   float TempToNDCX(double tempC, double presHPa) const
   {
      double logP  = std::log10(presHPa);
      double skewX = tempC - kSkewFactor * (logP - std::log10(kMaxPres));
      double plotW =
         (kMaxTemp - kMinTemp) +
         kSkewFactor * (std::log10(kMaxPres) - std::log10(kMinPres));
      return static_cast<float>(kNDCRange * (skewX - kMinTemp) / plotW -
                                kNDCOffset);
   }

   // Convert pressure to NDC y
   float PresToNDCY(double presHPa) const
   {
      double plotH = std::log10(kMaxPres) - std::log10(kMinPres);
      return static_cast<float>(
         kNDCRange * (std::log10(kMaxPres) - std::log10(presHPa)) / plotH -
         kNDCOffset);
   }

   void DrawLine(const std::vector<float>& vertices,
                 const std::vector<float>& colors)
   {
      if (vertices.empty())
      {
         return;
      }

      GLuint vao = 0, vbo = 0;
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
                   static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                   interleaved.data(),
                   GL_STREAM_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride7, nullptr);
      glEnableVertexAttribArray(0);

      glVertexAttribPointer(
         1,
         4,
         GL_FLOAT,
         GL_FALSE,
         kStride7,
         reinterpret_cast<void*>(static_cast<uintptr_t>(3 * sizeof(float))));
      glEnableVertexAttribArray(1);

      glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(count));

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawFilled(const std::vector<float>& vertices,
                   const std::vector<float>& colors)
   {
      if (vertices.empty())
      {
         return;
      }

      GLuint vao = 0, vbo = 0;
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
                   static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                   interleaved.data(),
                   GL_STREAM_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride7, nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(
         1,
         4,
         GL_FLOAT,
         GL_FALSE,
         kStride7,
         reinterpret_cast<void*>(static_cast<uintptr_t>(3 * sizeof(float))));
      glEnableVertexAttribArray(1);

      glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawPoint(float nx, float ny, float r, float g, float b, float a = 1.0f)
   {
      GLuint vao = 0, vbo = 0;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> ptData = {nx, ny, 0.0f, r, g, b, a};
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(ptData.size() * sizeof(float)),
                   ptData.data(),
                   GL_STREAM_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride7, nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(
         1,
         4,
         GL_FLOAT,
         GL_FALSE,
         kStride7,
         reinterpret_cast<void*>(static_cast<uintptr_t>(3 * sizeof(float))));
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
      static constexpr int    kStartTheta = -20;
      static constexpr int    kEndTheta   = 80;
      static constexpr int    kStepTheta  = 10;
      static constexpr double kAbsZero    = 273.15;

      std::vector<double> thetas;
      for (int t = kStartTheta; t <= kEndTheta; t += kStepTheta)
      {
         thetas.push_back(t + kAbsZero);
      }

      std::vector<std::vector<std::pair<double, double>>> lines;
      for (double theta : thetas)
      {
         std::vector<std::pair<double, double>> line;
         for (double p = kMinPres; p <= kMaxPres; p += 50)
         {
            // T = θ * (p / p0)^(R/Cp)
            double t = theta * std::pow(p / 1000.0, 0.286) - kAbsZero;
            line.push_back({t, p});
         }
         lines.push_back(line);
      }
      return lines;
   }

   // Precompute moist adiabats (pseudo-adiabatic)
   std::vector<std::vector<std::pair<double, double>>>
   ComputeMoistAdiabats() const
   {
      static constexpr int kStartSw = -20;
      static constexpr int kEndSw   = 40;
      static constexpr int kStepSw  = 10;

      std::vector<double> sw_temps;
      for (int t = kStartSw; t <= kEndSw; t += kStepSw)
      {
         sw_temps.push_back(t);
      }

      std::vector<std::vector<std::pair<double, double>>> lines;
      for (double startT : sw_temps)
      {
         std::vector<std::pair<double, double>> line;
         // Numerical integration of pseudo-adiabatic lapse rate
         double t = startT;
         for (double p = 1000.0; p >= kMinPres; p -= 25.0)
         {
            line.push_back({t, p});
            // Approximate lapse rate
            double dt_dp = 0.05; // Dummy placeholder for complex formula
            t += dt_dp * 25.0;
         }
         lines.push_back(line);
      }
      return lines;
   }

   // Constant mixing ratio lines
   std::vector<std::vector<std::pair<double, double>>>
   ComputeMixingRatioLines() const
   {
      std::vector<double> ratios = {1, 2, 4, 7, 10, 14, 20}; // g/kg
      std::vector<std::vector<std::pair<double, double>>> lines;
      for (double w : ratios)
      {
         std::vector<std::pair<double, double>> line;
         static constexpr double                kMixRatioMinP = 400.0;
         for (double p = kMixRatioMinP; p <= kMaxPres; p += 50)
         {
            // T = f(w, p) via Teten's or similar
            static constexpr double kEpsilon = 622.0;
            double                  es       = (w * p) / (kEpsilon + w);
            double                  t =
               243.5 * std::log(es / 6.112) / (17.67 - std::log(es / 6.112));
            line.push_back({t, p});
         }
         lines.push_back(line);
      }
      return lines;
   }

   void DrawGrid()
   {
      auto program = shader_;
      program->Use();

      // Isobars (Horizontal)
      static const std::vector<double> isobars = {
         1000, 850, 700, 500, 400, 300, 250, 200, 150, 100};
      for (double p : isobars)
      {
         float              ny    = PresToNDCY(p);
         float              nx1   = TempToNDCX(kMinTemp, p);
         float              nx2   = TempToNDCX(kMaxTemp, p);
         std::vector<float> verts = {nx1, ny, 0.0f, nx2, ny, 0.0f};
         std::vector<float> cols  = {
            0.3f, 0.3f, 0.4f, 0.5f, 0.3f, 0.3f, 0.4f, 0.5f};
         DrawLine(verts, cols);
      }

      // Isotherms (Diagonal due to skew)
      for (int t = -100; t <= 40; t += 10)
      {
         float              ny1   = PresToNDCY(kMaxPres);
         float              ny2   = PresToNDCY(kMinPres);
         float              nx1   = TempToNDCX(t, kMaxPres);
         float              nx2   = TempToNDCX(t, kMinPres);
         std::vector<float> verts = {nx1, ny1, 0.0f, nx2, ny2, 0.0f};
         std::vector<float> cols  = {
            0.4f, 0.3f, 0.3f, 0.4f, 0.4f, 0.3f, 0.3f, 0.4f};
         if (t == 0)
         {
            cols = {0.2f,
                    0.6f,
                    0.8f,
                    0.6f,
                    0.2f,
                    0.6f,
                    0.8f,
                    0.6f}; // Freezing line - Blueish
         }
         DrawLine(verts, cols);
      }
   }

   void DrawReferenceLines()
   {
      // Dry Adiabats
      auto dry = ComputeDryAdiabats();
      for (const auto& line : dry)
      {
         std::vector<float> verts, cols;
         for (const auto& pt : line)
         {
            verts.insert(
               verts.end(),
               {TempToNDCX(pt.first, pt.second), PresToNDCY(pt.second), 0.0f});
            cols.insert(cols.end(), {0.5f, 0.5f, 0.2f, 0.2f});
         }
         DrawLine(verts, cols);
      }

      // Moist Adiabats
      auto moist = ComputeMoistAdiabats();
      for (const auto& line : moist)
      {
         std::vector<float> verts, cols;
         for (const auto& pt : line)
         {
            verts.insert(
               verts.end(),
               {TempToNDCX(pt.first, pt.second), PresToNDCY(pt.second), 0.0f});
            cols.insert(cols.end(), {0.2f, 0.5f, 0.2f, 0.2f});
         }
         DrawLine(verts, cols);
      }

      // Mixing Ratio
      auto mix = ComputeMixingRatioLines();
      for (const auto& line : mix)
      {
         std::vector<float> verts, cols;
         for (const auto& pt : line)
         {
            verts.insert(
               verts.end(),
               {TempToNDCX(pt.first, pt.second), PresToNDCY(pt.second), 0.0f});
            cols.insert(cols.end(), {0.5f, 0.3f, 0.1f, 0.2f});
         }
         DrawLine(verts, cols);
      }
   }

   void DrawProfiles()
   {
      if (!sounding_)
         return;

      const auto& levels = sounding_->levels();
      if (levels.empty())
         return;

      // Temperature profile (thick red)
      std::vector<float>      t_verts, t_cols;
      static constexpr double kProfileLimitT = -150.0;
      for (const auto& lvl : levels)
      {
         if (lvl.temperature_C_ < kProfileLimitT)
            continue;
         t_verts.insert(t_verts.end(),
                        {TempToNDCX(lvl.temperature_C_, lvl.pressure_hPa_),
                         PresToNDCY(lvl.pressure_hPa_),
                         0.0f});
         t_cols.insert(t_cols.end(), {1.0f, 0.2f, 0.2f, 1.0f});
      }
      DrawLine(t_verts, t_cols);

      // Dewpoint profile (thick green)
      std::vector<float> d_verts, d_cols;
      for (const auto& lvl : levels)
      {
         if (lvl.dewpoint_C_ < kProfileLimitT)
            continue;
         d_verts.insert(d_verts.end(),
                        {TempToNDCX(lvl.dewpoint_C_, lvl.pressure_hPa_),
                         PresToNDCY(lvl.pressure_hPa_),
                         0.0f});
         d_cols.insert(d_cols.end(),
                       {0.2f, 1.0f, 0.2f, 1.0f}); // Brighter green
      }
      DrawLine(d_verts, d_cols);
   }

   void DrawCapeCin()
   {
      if (!sounding_)
         return;

      const auto& levels = sounding_->levels();
      const auto& parcel = sounding_->parcel_profile();
      if (parcel.empty())
         return;

      std::vector<float> verts, cols;
      for (size_t i = 0; i < parcel.size(); ++i)
      {
         double p       = parcel[i].pressure_hPa_;
         double parcelT = parcel[i].temperature_C_;

         // Find environmental T at this pressure
         auto it = std::lower_bound(levels.begin(),
                                    levels.end(),
                                    p,
                                    [](const auto& lvl, double val)
                                    { return lvl.pressure_hPa_ > val; });

         if (it != levels.end() && it != levels.begin())
         {
            auto   prev  = std::prev(it);
            double alpha = (p - prev->pressure_hPa_) /
                           (it->pressure_hPa_ - prev->pressure_hPa_);
            double envT = prev->temperature_C_ +
                          alpha * (it->temperature_C_ - prev->temperature_C_);

            float nxP = TempToNDCX(parcelT, p);
            float nxE = TempToNDCX(envT, p);
            float ny  = PresToNDCY(p);

            verts.insert(verts.end(), {nxE, ny, 0.0f, nxP, ny, 0.0f});
            if (parcelT > envT)
            {
               // CAPE (Red/Orange fill)
               cols.insert(cols.end(),
                           {1.0f, 0.5f, 0.0f, 0.3f, 1.0f, 0.5f, 0.0f, 0.3f});
            }
            else
            {
               // CIN (Blue fill)
               cols.insert(cols.end(),
                           {0.2f, 0.4f, 1.0f, 0.3f, 0.2f, 0.4f, 1.0f, 0.3f});
            }
         }
      }

      // Convert pairs to triangles
      std::vector<float> triVerts, triCols;
      for (size_t i = 0; i + 1 < verts.size() / 6; ++i)
      {
         // Triangle 1
         triVerts.insert(triVerts.end(),
                         {verts[i * 6], verts[i * 6 + 1], 0.0f});
         triVerts.insert(triVerts.end(),
                         {verts[i * 6 + 3], verts[i * 6 + 4], 0.0f});
         triVerts.insert(triVerts.end(),
                         {verts[(i + 1) * 6], verts[(i + 1) * 6 + 1], 0.0f});

         // Triangle 2
         triVerts.insert(triVerts.end(),
                         {verts[i * 6 + 3], verts[i * 6 + 4], 0.0f});
         triVerts.insert(
            triVerts.end(),
            {verts[(i + 1) * 6 + 3], verts[(i + 1) * 6 + 4], 0.0f});
         triVerts.insert(triVerts.end(),
                         {verts[(i + 1) * 6], verts[(i + 1) * 6 + 1], 0.0f});

         for (int j = 0; j < 6; ++j)
            triCols.insert(triCols.end(),
                           {cols[i * 8],
                            cols[i * 8 + 1],
                            cols[i * 8 + 2],
                            cols[i * 8 + 3]});
      }
      DrawFilled(triVerts, triCols);
   }

   void DrawParcelProfile()
   {
      if (!sounding_)
         return;
      const auto& parcel = sounding_->parcel_profile();
      if (parcel.empty())
         return;

      std::vector<float> verts, cols;
      for (const auto& pt : parcel)
      {
         verts.insert(verts.end(),
                      {TempToNDCX(pt.temperature_C_, pt.pressure_hPa_),
                       PresToNDCY(pt.pressure_hPa_),
                       0.0f});
         cols.insert(cols.end(), {1.0f, 1.0f, 1.0f, 0.8f});
      }
      DrawLine(verts, cols);
   }

   void DrawMarkers()
   {
      if (!sounding_)
         return;
      // LCL, LFC, EL markers
      auto drawMarker = [&](double p, float r, float g, float b)
      {
         if (p <= 0)
            return;
         float ny = PresToNDCY(p);
         DrawPoint(TempToNDCX(0, p), ny, r, g, b); // Marker at T=0
      };

      drawMarker(sounding_->lcl_pressure_hPa(), 1, 1, 0);
      drawMarker(sounding_->lfc_pressure_hPa(), 1, 0.5f, 0);
      drawMarker(sounding_->el_pressure_hPa(), 1, 0, 1);
   }

   void DrawSingleWindBarb(float x, float y, double speedMps, double dirDeg)
   {
      static constexpr double kMpsToKnots = 1.94384;
      double                  speedKt     = speedMps * kMpsToKnots;
      double                  angRad =
         (dirDeg + 90.0) * (M_PI / 180.0); // Barb points INTO the wind

      float dx = static_cast<float>(std::cos(angRad));
      float dy = static_cast<float>(std::sin(angRad));

      // Main stem
      float              x2        = x + dx * kBarbStemLength;
      float              y2        = y + dy * kBarbStemLength;
      std::vector<float> stemVerts = {x, y, 0.0f, x2, y2, 0.0f};
      std::vector<float> stemCols  = {1, 1, 1, 1, 1, 1, 1, 1};
      DrawLine(stemVerts, stemCols);

      // Ticks and Pennants
      // Pennants (50 kt), Full ticks (10 kt), Half ticks (5 kt)
      int num50 = static_cast<int>(speedKt) / 50;
      int num10 = (static_cast<int>(speedKt) % 50) / 10;
      int num5  = (static_cast<int>(speedKt) % 10) / 5;

      float  currentPos = kBarbStemLength;
      double tickAngRad = (dirDeg + 90.0 + kBarbTickAngle) * (M_PI / 180.0);
      float  tdx        = static_cast<float>(std::cos(tickAngRad));
      float  tdy        = static_cast<float>(std::sin(tickAngRad));

      auto drawTick = [&](float length, bool isPennant)
      {
         float tx1 = x + dx * currentPos;
         float ty1 = y + dy * currentPos;
         float tx2 = tx1 + tdx * length;
         float ty2 = ty1 + tdy * length;

         if (isPennant)
         {
            float              tx3    = x + dx * (currentPos - kBarbTickStep);
            float              ty3    = y + dy * (currentPos - kBarbTickStep);
            std::vector<float> pVerts = {
               tx1, ty1, 0.0f, tx2, ty2, 0.0f, tx3, ty3, 0.0f};
            std::vector<float> pCols = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
            DrawFilled(pVerts, pCols);
            currentPos -= kBarbTickStep;
         }
         else
         {
            std::vector<float> tVerts = {tx1, ty1, 0.0f, tx2, ty2, 0.0f};
            std::vector<float> tCols  = {1, 1, 1, 1, 1, 1, 1, 1};
            DrawLine(tVerts, tCols);
         }
         currentPos -= kBarbTickStep;
      };

      for (int i = 0; i < num50; ++i)
         drawTick(kBarbTickLength, true);
      for (int i = 0; i < num10; ++i)
         drawTick(kBarbTickLength, false);
      if (num5 > 0)
         drawTick(kBarbTickLength * 0.5f, false);
   }

   void DrawWindBarbs()
   {
      if (!sounding_)
         return;

      const auto&             levels = sounding_->levels();
      static constexpr size_t kBarbInterval =
         8; // More spacing for larger barbs

      for (size_t i = 0; i < levels.size(); i += kBarbInterval)
      {
         const auto& lvl = levels[i];
         if (lvl.wind_speed_mps_ < 1.0)
            continue; // Calm

         float ny = PresToNDCY(lvl.pressure_hPa_);
         DrawSingleWindBarb(
            kBarbX, ny, lvl.wind_speed_mps_, lvl.wind_direction_deg_);
      }
   }

   void DrawAxisLabels(QPainter& painter)
   {
      painter.setPen(Qt::white);
      QFont labelFont = painter.font();
      static constexpr int kAxisFontSize = 8;
      labelFont.setPointSize(kAxisFontSize);
      painter.setFont(labelFont);

      // Pressure labels
      static const std::vector<double> isobars = {
         1000, 850, 700, 500, 400, 300, 200, 100};
      for (double p : isobars)
      {
         float                ny = PresToNDCY(p);
         int                  y  = static_cast<int>((1.0f - ny) / 2.0f *
                                  static_cast<float>(rect_.height()));
         static constexpr int kLabelOffsetX = 10;
         static constexpr int kLabelOffsetY = 5;
         painter.drawText(kLabelOffsetX,
                          y + kLabelOffsetY,
                          QString::number(static_cast<int>(p)));
      }

      // Temperature labels
      for (int t = -60; t <= 40; t += 20)
      {
         float                nx = TempToNDCX(t, kMaxPres);
         int                  x  = static_cast<int>((nx + 1.0f) / 2.0f *
                                  static_cast<float>(rect_.width()));
         static constexpr int kLabelOffsetX = 10;
         static constexpr int kLabelOffsetY = 10;
         painter.drawText(x - kLabelOffsetX,
                          rect_.height() - kLabelOffsetY,
                          QString::number(t));
      }
   }

   void Render()
   {
      if (!shaderLoaded_)
      {
         return;
      }

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

   SkewtWidget*                            widget_;
   std::shared_ptr<gl::ShaderProgram>      shader_;
   glm::mat4                               projMatrix_ {};
   std::shared_ptr<sounding::SoundingData> sounding_;
   QRect                                   rect_ {};
   bool                                    shaderLoaded_ {false};

   double    hoverPressure_ {0.0};
   QPoint    hoverPos_ {};
   glm::vec2 hoverNDC_ {};
};

SkewtWidget::SkewtWidget(QWidget* parent) :
    QOpenGLWidget(parent), p(std::make_unique<Impl>(this))
{
   static constexpr int kMinimumSize = 300;
   setMinimumSize(kMinimumSize, kMinimumSize);
   setMouseTracking(true);
}
SkewtWidget::~SkewtWidget() = default;

void SkewtWidget::SetSounding(
   const std::shared_ptr<sounding::SoundingData>& sounding)
{
   p->sounding_ = std::move(sounding);
   update();
}

void SkewtWidget::SetHoverLevel(double pressureHPa)
{
   if (p->hoverPressure_ != pressureHPa)
   {
      p->hoverPressure_ = pressureHPa;
      update();
   }
}

void SkewtWidget::mouseMoveEvent(QMouseEvent* event)
{
   p->hoverPos_ = event->pos();

   // Convert pixel to NDC
   float nx = (2.0f * static_cast<float>(event->position().x()) /
               static_cast<float>(width())) -
              1.0f;
   float ny     = 1.0f - (2.0f * static_cast<float>(event->position().y()) /
                      static_cast<float>(height()));
   p->hoverNDC_ = {nx, ny};

   double temp = 0.0, pres = 0.0;
   p->NDCToSkewT(nx, ny, temp, pres);
   p->hoverPressure_ = pres;

   Q_EMIT LevelHovered(pres);
   update();
}

void SkewtWidget::leaveEvent(QEvent* /*event*/)
{
   p->hoverPressure_ = 0;
   Q_EMIT LevelHovered(0);
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

   // Draw hover line (OpenGL)
   p->DrawHoverLine();

   QPainter painter(this);
   p->DrawAxisLabels(painter);

   // Draw tooltip (QPainter)
   p->DrawTooltip(painter);

   painter.end();
}

} // namespace scwx::qt::view
