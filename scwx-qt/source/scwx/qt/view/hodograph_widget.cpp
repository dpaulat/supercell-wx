#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/view/hodograph_widget.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>
#include <numbers>
#include <cmath>

#define _USE_MATH_DEFINES
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <QMouseEvent>
#include <QPainter>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx::qt::view
{

static const std::string logPrefix_ = "scwx::qt::view::hodograph_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static constexpr float                kMaxWindDefault = 10.0f;
static constexpr float                kPlotScale      = 0.85f;
static constexpr std::array<float, 4> kGridColor {0.35f, 0.35f, 0.5f, 0.6f};
static constexpr std::array<float, 4> kBackgroundColor {
   0.08f, 0.08f, 0.15f, 1.0f};
static constexpr double kDegToRad = std::numbers::pi / 180.0;

static constexpr int                kTooltipOffsetX    = 10;
static constexpr int                kTooltipOffset     = 10;
static constexpr float              kMaxWindPadding    = 1.2f;
static constexpr float              kSpeedRingInterval = 10.0f;
static constexpr int                kSpokeInterval     = 30;
static constexpr int                kLabelFontSize     = 7;
static constexpr std::array<int, 3> kAxisLabelColor {180, 180, 200};

static constexpr float kNDCScale  = 2.0f;
static constexpr float kNDCOffset = 1.0f;

static constexpr int kBackgroundAlpha  = 160;
static constexpr int kVerticesPerPoint = 2;
static constexpr int kCoordsPerVertex  = 3;
static constexpr int kColorsPerVertex  = 4;
static constexpr int kStride7 =
   (kCoordsPerVertex + kColorsPerVertex) * sizeof(float);
static constexpr size_t kColorOffset = kCoordsPerVertex * sizeof(float);

template<typename T>
static inline const void* BufferOffset(T offset)
{
   return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
}

class HodographWidget::Impl
{
public:
   explicit Impl(HodographWidget* widget) : widget_(widget) {}
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   // Convert NDC back to wind U/V
   void NDCToWind(float nx, float ny, double& u, double& v) const
   {
      double maxWind =
         std::max(maxWindSpeed_, static_cast<double>(kMaxWindDefault));
      u = nx / kPlotScale * maxWind;
      v = ny / kPlotScale * maxWind;
   }

   void DrawHoverPoint()
   {
      if (hoverPressure_ <= 0 || !sounding_)
         return;

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1,
                         GL_FALSE,
                         glm::value_ptr(projMatrix_));

      // Find wind at hover pressure
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

      double u =
         -it->wind_speed_mps_ * std::sin(it->wind_direction_deg_ * kDegToRad);
      double v =
         -it->wind_speed_mps_ * std::cos(it->wind_direction_deg_ * kDegToRad);

      float nx = 0.0f, ny = 0.0f;
      WindToNDC(u, v, nx, ny);

      GLuint vao = 0, vbo = 0;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      static constexpr float kPointColorR = 1.0f;
      static constexpr float kPointColorG = 1.0f;
      static constexpr float kPointColorB = 1.0f;
      static constexpr float kPointColorA = 1.0f;
      std::vector<float>     ptData       = {
         nx, ny, 0.0f, kPointColorR, kPointColorG, kPointColorB, kPointColorA};
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(ptData.size() * sizeof(float)),
                   ptData.data(),
                   GL_STREAM_DRAW);

      static constexpr GLsizei kStride = kStride7;
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride, nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(
         1, 4, GL_FLOAT, GL_FALSE, kStride, BufferOffset(kColorOffset));
      glEnableVertexAttribArray(1);

      static constexpr float kHoverPointSize = 10.0f;
      glPointSize(kHoverPointSize);
      glDrawArrays(GL_POINTS, 0, 1);

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawTooltip(QPainter& painter)
   {
      if (hoverPressure_ <= 0 || !sounding_)
         return;

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

      QString text = QString("P: %1 hPa\nZ: %2 m\nW: %3 m/s\nDir: %4°")
                        .arg(it->pressure_hPa_, 0, 'f', 1)
                        .arg(it->height_m_, 0, 'f', 0)
                        .arg(it->wind_speed_mps_, 0, 'f', 1)
                        .arg(it->wind_direction_deg_, 0, 'f', 0);

      painter.setPen(Qt::white);
      painter.setBackground(QBrush(QColor(0, 0, 0, kBackgroundAlpha)));
      painter.setBackgroundMode(Qt::OpaqueMode);
      painter.drawText(
         hoverPos_.x() + kTooltipOffsetX, hoverPos_.y() - kTooltipOffset, text);
   }

   // Convert wind U/V to NDC
   void WindToNDC(double u, double v, float& nx, float& ny) const
   {
      // Hodograph coordinate: u=x, v=y
      // Scale to fit within [-0.9, 0.9] NDC based on max wind
      double maxWind =
         std::max(maxWindSpeed_, static_cast<double>(kMaxWindDefault));
      nx = static_cast<float>(u / maxWind * kPlotScale);
      ny = static_cast<float>(v / maxWind * kPlotScale);
   }

   void DrawLine(const std::vector<float>& vertices,
                 const std::vector<float>& colors)
   {
      if (vertices.size() < static_cast<size_t>(kVerticesPerPoint) *
                               static_cast<size_t>(kCoordsPerVertex))
      {
         return;
      }

      GLuint vao = 0, vbo = 0;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> interleaved;
      size_t             count = vertices.size() / kCoordsPerVertex;
      interleaved.reserve(count * (kCoordsPerVertex + kColorsPerVertex));
      for (size_t i = 0; i < count; ++i)
      {
         interleaved.push_back(vertices[i * kCoordsPerVertex]);
         interleaved.push_back(vertices[i * kCoordsPerVertex + 1]);
         interleaved.push_back(vertices[i * kCoordsPerVertex + 2]);
         for (int c = 0; c < kColorsPerVertex; ++c)
         {
            interleaved.push_back(colors[i * kColorsPerVertex + c]);
         }
      }

      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                   interleaved.data(),
                   GL_STREAM_DRAW);

      static constexpr GLsizei kStride = kStride7;
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride, nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(
         1, 4, GL_FLOAT, GL_FALSE, kStride, BufferOffset(kColorOffset));
      glEnableVertexAttribArray(1);

      glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(count));

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawPoints(const std::vector<float>& vertices,
                   const std::vector<float>& colors)
   {
      if (vertices.size() < static_cast<size_t>(kVerticesPerPoint) *
                               static_cast<size_t>(kCoordsPerVertex))
      {
         return;
      }

      GLuint vao = 0, vbo = 0;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> interleaved;
      size_t             count = vertices.size() / kCoordsPerVertex;
      interleaved.reserve(count * (kCoordsPerVertex + kColorsPerVertex));
      for (size_t i = 0; i < count; ++i)
      {
         interleaved.push_back(vertices[i * kCoordsPerVertex]);
         interleaved.push_back(vertices[i * kCoordsPerVertex + 1]);
         interleaved.push_back(vertices[i * kCoordsPerVertex + 2]);
         for (int c = 0; c < kColorsPerVertex; ++c)
         {
            interleaved.push_back(colors[i * kColorsPerVertex + c]);
         }
      }

      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                   interleaved.data(),
                   GL_STREAM_DRAW);

      static constexpr GLsizei kStride = kStride7;
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride, nullptr);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(
         1, 4, GL_FLOAT, GL_FALSE, kStride, BufferOffset(kColorOffset));
      glEnableVertexAttribArray(1);

      static constexpr float kDefaultPointSize = 6.0f;
      glPointSize(kDefaultPointSize);
      glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(count));

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawGrid()
   {
      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1,
                         GL_FALSE,
                         glm::value_ptr(projMatrix_));

      double maxWind =
         std::max(maxWindSpeed_, static_cast<double>(kMaxWindDefault));

      // Speed rings (concentric circles)
      int numRings = static_cast<int>(std::ceil(maxWind / kSpeedRingInterval));
      for (int r = 1; r <= numRings; ++r)
      {
         double speed = static_cast<double>(r) * kSpeedRingInterval;
         if (speed > maxWind * kMaxWindPadding)
         {
            break;
         }

         float radius = static_cast<float>(speed / (maxWind * kMaxWindPadding) *
                                           kPlotScale);

         std::vector<float> verts, cols;
         static constexpr int kSegments = 36;
         for (int i = 0; i <= kSegments; ++i)
         {
            static constexpr double kTwoPi = 2.0 * std::numbers::pi;
            double                  ang    = kTwoPi * i / kSegments;
            float x = static_cast<float>(radius * std::sin(ang));
            float y = static_cast<float>(radius * std::cos(ang));
            verts.insert(verts.end(), {x, y, 0.0f});
            cols.insert(
               cols.end(),
               {kGridColor[0], kGridColor[1], kGridColor[2], kGridColor[3]});
         }
         DrawLine(verts, cols);
      }

      // Direction spokes (every 30 degrees)
      static constexpr int kSpokeLimit = 360;
      for (int d = 0; d < kSpokeLimit; d += kSpokeInterval)
      {
         double ang = d * kDegToRad;
         float  len = kPlotScale;

         std::vector<float> verts = {0.0f,
                                     0.0f,
                                     0.0f,
                                     static_cast<float>(len * std::sin(ang)),
                                     static_cast<float>(len * std::cos(ang)),
                                     0.0f};
         std::vector<float> cols  = {kGridColor[0],
                                     kGridColor[1],
                                     kGridColor[2],
                                     kGridColor[3],
                                     kGridColor[0],
                                     kGridColor[1],
                                     kGridColor[2],
                                     kGridColor[3]};
         DrawLine(verts, cols);
      }
   }

   void DrawWindProfile()
   {
      if (!sounding_ || sounding_->levels().empty())
      {
         return;
      }

      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1,
                         GL_FALSE,
                         glm::value_ptr(projMatrix_));

      auto levels = sounding_->levels();
      std::sort(levels.begin(),
                levels.end(),
                [](const auto& a, const auto& b)
                { return a.pressure_hPa_ > b.pressure_hPa_; });

      maxWindSpeed_ = 0.0;
      for (const auto& lvl : levels)
      {
         maxWindSpeed_ = std::max(maxWindSpeed_, lvl.wind_speed_mps_);
      }

      // Wind profile line (color-coded by height)
      std::vector<float> verts, cols;
      for (const auto& lvl : levels)
      {
         // Negate: meteorological wind direction (coming FROM) → hodograph
         // vector (blowing TOWARD)
         double u = -lvl.wind_speed_mps_ *
                    std::sin(lvl.wind_direction_deg_ * kDegToRad);
         double v = -lvl.wind_speed_mps_ *
                    std::cos(lvl.wind_direction_deg_ * kDegToRad);

         float nx = 0.0f, ny = 0.0f;
         WindToNDC(u, v, nx, ny);
         verts.insert(verts.end(), {nx, ny, 0.0f});

         // Color by height (Standard layers)
         static constexpr double kMToKm   = 1000.0;
         double                  heightKm = lvl.height_m_ / kMToKm;
         float                   r = 1.0f, g = 1.0f, b = 1.0f;
         static constexpr double kHeightThreshold3  = 3.0;
         static constexpr double kHeightThreshold6  = 6.0;
         static constexpr double kHeightThreshold9  = 9.0;
         static constexpr double kHeightThreshold12 = 12.0;

         if (heightKm < kHeightThreshold3)
         {
            // 0-3km: Red to Orange
            static constexpr float kColorSurfaceG = 0.1f;
            static constexpr float kColorSurfaceB = 0.1f;
            static constexpr float kColorRangeG   = 0.4f;

            r = 1.0f;
            g = static_cast<float>(
               kColorSurfaceG + (heightKm / kHeightThreshold3) * kColorRangeG);
            b = kColorSurfaceB;
         }
         else if (heightKm < kHeightThreshold6)
         {
            // 3-6km: Green
            static constexpr float kColorLayer6R = 0.1f;
            static constexpr float kColorLayer6G = 0.8f;
            static constexpr float kColorLayer6B = 0.1f;

            r = kColorLayer6R;
            g = kColorLayer6G;
            b = kColorLayer6B;
         }
         else if (heightKm < kHeightThreshold9)
         {
            // 6-9km: Blue
            static constexpr float kColorLayer9R = 0.1f;
            static constexpr float kColorLayer9G = 0.4f;
            static constexpr float kColorLayer9B = 1.0f;

            r = kColorLayer9R;
            g = kColorLayer9G;
            b = kColorLayer9B;
         }
         else if (heightKm < kHeightThreshold12)
         {
            // 9-12km: Purple
            static constexpr float kColorLayer12R = 0.8f;
            static constexpr float kColorLayer12G = 0.1f;
            static constexpr float kColorLayer12B = 0.8f;

            r = kColorLayer12R;
            g = kColorLayer12G;
            b = kColorLayer12B;
         }

         cols.insert(cols.end(), {r, g, b, 1.0f});
      }

      DrawLine(verts, cols);

      // Points at each level
      DrawPoints(verts, cols);
   }

   void DrawAxisLabels(QPainter& painter)
   {
      if (!sounding_)
      {
         return;
      }

      painter.setRenderHint(QPainter::Antialiasing);

      double maxWind =
         std::max(maxWindSpeed_, static_cast<double>(kMaxWindDefault));
      int numRings = static_cast<int>(std::ceil(maxWind / kSpeedRingInterval));

      painter.setPen(
         QColor(kAxisLabelColor[0], kAxisLabelColor[1], kAxisLabelColor[2]));
      QFont labelFont = painter.font();
      labelFont.setPointSize(kLabelFontSize);
      painter.setFont(labelFont);

      // Speed labels on right side
      for (int r = 1; r <= numRings; ++r)
      {
         double speed = static_cast<double>(r) * kSpeedRingInterval;
         if (speed > maxWind * kMaxWindPadding)
         {
            break;
         }

         float radius = static_cast<float>(speed / (maxWind * kMaxWindPadding) *
                                           kPlotScale);

         // Position label at top of each ring (90 degrees)
         static constexpr float kLabelPaddingX = 3.0f;
         static constexpr float kLabelPaddingY = 0.02f;
         QPointF                labelPos((radius + kNDCOffset) / kNDCScale *
                                static_cast<float>(rect_.width()) +
                             kLabelPaddingX,
                          (kNDCOffset - radius - kLabelPaddingY) / kNDCScale *
                             static_cast<float>(rect_.height()));
         painter.drawText(labelPos, QString::number(static_cast<int>(speed)));
      }

      // Cardinal direction labels
      painter.setPen(
         QColor(kAxisLabelColor[0], kAxisLabelColor[1], kAxisLabelColor[2]));
      auto drawDirLabel =
         [&](const char* text, double deg, double offsetX, double offsetY)
      {
         double                  ang          = deg * kDegToRad;
         static constexpr double kLabelRadius = 0.92;
         double                  len          = kLabelRadius;
         QPointF pos((len * std::sin(ang) + static_cast<double>(kNDCOffset)) /
                           static_cast<double>(kNDCScale) * rect_.width() +
                        offsetX,
                     (static_cast<double>(kNDCOffset) - len * std::cos(ang)) /
                           static_cast<double>(kNDCScale) * rect_.height() +
                        offsetY);
         painter.drawText(pos, text);
      };

      static constexpr double kDirLabelOffset = 5.0;
      static constexpr double kDirN           = 0.0;
      static constexpr double kDirE           = 90.0;
      static constexpr double kDirS           = 180.0;
      static constexpr double kDirW           = 270.0;

      drawDirLabel("N", kDirN, -kDirLabelOffset, kDirLabelOffset);
      drawDirLabel("E", kDirE, -kDirLabelOffset, kDirLabelOffset);
      drawDirLabel("S", kDirS, -kDirLabelOffset, kDirLabelOffset);
      drawDirLabel("W", kDirW, -kDirLabelOffset, kDirLabelOffset);
   }

   void Render()
   {
      if (!shaderLoaded_)
      {
         return;
      }

      glClearColor(kBackgroundColor[0],
                   kBackgroundColor[1],
                   kBackgroundColor[2],
                   kBackgroundColor[3]);
      glClear(GL_COLOR_BUFFER_BIT);

      projMatrix_ = glm::ortho(-kNDCOffset,
                               kNDCOffset,
                               -kNDCOffset,
                               kNDCOffset,
                               -kNDCOffset,
                               kNDCOffset);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      DrawGrid();
      DrawWindProfile();

      glDisable(GL_BLEND);
   }

   HodographWidget*                        widget_;
   std::shared_ptr<gl::ShaderProgram>      shader_;
   glm::mat4                               projMatrix_ {};
   std::shared_ptr<sounding::SoundingData> sounding_;
   QRect                                   rect_ {};
   double                                  maxWindSpeed_ {kMaxWindDefault};
   bool                                    shaderLoaded_ {false};

   double hoverPressure_ {0.0};
   QPoint hoverPos_ {};
};

HodographWidget::HodographWidget(QWidget* parent) :
    QOpenGLWidget(parent), p(std::make_unique<Impl>(this))
{
   static constexpr int kMinimumSize = 300;
   setMinimumSize(kMinimumSize, kMinimumSize);
   setMouseTracking(true);
}
HodographWidget::~HodographWidget() = default;

void HodographWidget::SetSounding(
   const std::shared_ptr<sounding::SoundingData>& sounding)
{
   p->sounding_ = sounding;
   update();
}

void HodographWidget::SetHoverLevel(double pressureHPa)
{
   if (p->hoverPressure_ != pressureHPa)
   {
      p->hoverPressure_ = pressureHPa;
      update();
   }
}

void HodographWidget::mouseMoveEvent(QMouseEvent* event)
{
   p->hoverPos_ = event->pos();

   if (!p->sounding_)
      return;

   // Convert pixel to NDC
   float nx = (kNDCScale * static_cast<float>(event->position().x()) /
               static_cast<float>(width())) -
              kNDCOffset;
   float ny =
      kNDCOffset - (kNDCScale * static_cast<float>(event->position().y()) /
                    static_cast<float>(height()));

   double u = 0.0, v = 0.0;
   p->NDCToWind(nx, ny, u, v);

   // Find closest level on trace
   const auto& levels = p->sounding_->levels();
   auto        it     = std::min_element(
      levels.begin(),
      levels.end(),
      [&](const auto& a, const auto& b)
      {
         double u_a =
            -a.wind_speed_mps_ * std::sin(a.wind_direction_deg_ * kDegToRad);
         double v_a =
            -a.wind_speed_mps_ * std::cos(a.wind_direction_deg_ * kDegToRad);
         double u_b =
            -b.wind_speed_mps_ * std::sin(b.wind_direction_deg_ * kDegToRad);
         double v_b =
            -b.wind_speed_mps_ * std::cos(b.wind_direction_deg_ * kDegToRad);
         double distSqA = (u - u_a) * (u - u_a) + (v - v_a) * (v - v_a);
         double distSqB = (u - u_b) * (u - u_b) + (v - v_b) * (v - v_b);
         return distSqA < distSqB;
      });

   if (it != levels.end())
   {
      p->hoverPressure_ = it->pressure_hPa_;
      Q_EMIT LevelHovered(it->pressure_hPa_);
   }

   update();
}

void HodographWidget::leaveEvent(QEvent* /*event*/)
{
   p->hoverPressure_ = 0;
   Q_EMIT LevelHovered(0);
   update();
}

void HodographWidget::initializeGL()
{
   p->shader_ = std::make_shared<gl::ShaderProgram>();
   if (!p->shader_->Load(":/gl/color.vert", ":/gl/color.frag"))
   {
      logger_->error("Failed to load hodograph shaders");
      return;
   }

   p->shaderLoaded_ = true;
   logger_->debug("HodographWidget OpenGL initialized");
}

void HodographWidget::resizeGL(int w, int h)
{
   p->rect_ = QRect(0, 0, w, h);
   glViewport(0, 0, w, h);
}

void HodographWidget::paintGL()
{
   p->Render();

   // Draw hover point (OpenGL)
   p->DrawHoverPoint();

   QPainter painter(this);
   p->DrawAxisLabels(painter);

   // Draw tooltip (QPainter)
   p->DrawTooltip(painter);

   painter.end();
}

} // namespace scwx::qt::view
