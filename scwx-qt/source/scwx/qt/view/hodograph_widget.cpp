#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/view/hodograph_widget.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <vector>

#include <algorithm>
#include <vector>

#include <QPainter>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx::qt::view
{

static const std::string logPrefix_ = "scwx::qt::view::hodograph_widget";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class HodographWidget::Impl
{
public:
   explicit Impl(HodographWidget* widget) :
       widget_(widget),
       shaderLoaded_(false)
   {
   }
   ~Impl() = default;

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   // Convert wind U/V to NDC
   void WindToNDC(double u, double v, float& nx, float& ny) const
   {
      // Hodograph coordinate: u=x, v=y
      // Scale to fit within [-0.9, 0.9] NDC based on max wind
      double maxWind = std::max(maxWindSpeed_, 10.0);
      nx = static_cast<float>(u / maxWind * 0.85);
      ny = static_cast<float>(v / maxWind * 0.85);
   }

   void DrawLine(const std::vector<float>& vertices,
                 const std::vector<float>& colors)
   {
      if (vertices.size() < 6) { return; }

      GLuint vao, vbo;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> interleaved;
      size_t count = vertices.size() / 3;
      interleaved.reserve(count * 7);
      for (size_t i = 0; i < count; ++i)
      {
         interleaved.push_back(vertices[i * 3]);
         interleaved.push_back(vertices[i * 3 + 1]);
         interleaved.push_back(vertices[i * 3 + 2]);
         for (int c = 0; c < 4; ++c)
         {
            interleaved.push_back(colors[i * 4 + c]);
         }
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

   void DrawPoints(const std::vector<float>& vertices,
                   const std::vector<float>& colors)
   {
      if (vertices.size() < 6) { return; }

      GLuint vao, vbo;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);

      std::vector<float> interleaved;
      size_t count = vertices.size() / 3;
      interleaved.reserve(count * 7);
      for (size_t i = 0; i < count; ++i)
      {
         interleaved.push_back(vertices[i * 3]);
         interleaved.push_back(vertices[i * 3 + 1]);
         interleaved.push_back(vertices[i * 3 + 2]);
         for (int c = 0; c < 4; ++c)
         {
            interleaved.push_back(colors[i * 4 + c]);
         }
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

      glPointSize(6.0f);
      glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(count));

      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
   }

   void DrawGrid()
   {
      auto program = shader_;
      program->Use();
      glUniformMatrix4fv(program->GetUniformLocation("uMVPMatrix"),
                         1, GL_FALSE, glm::value_ptr(projMatrix_));

      double maxWind = std::max(maxWindSpeed_, 10.0);

      // Speed rings (concentric circles)
      int numRings = static_cast<int>(std::ceil(maxWind / 10.0));
      for (int r = 1; r <= numRings; ++r)
      {
         double speed = r * 10.0;
         if (speed > maxWind * 1.2) { break; }

         float radius = static_cast<float>(speed / (maxWind * 1.2) * 0.85);

         std::vector<float> verts, cols;
         int segments = 36;
         for (int i = 0; i <= segments; ++i)
         {
            double ang = 2.0 * M_PI * i / segments;
            float x    = radius * std::sin(ang);
            float y    = radius * std::cos(ang);
            verts.insert(verts.end(), {x, y, 0.0f});
            cols.insert(cols.end(), {0.35f, 0.35f, 0.5f, 0.6f});
         }
         DrawLine(verts, cols);
      }

      // Direction spokes (every 30 degrees)
      for (int d = 0; d < 360; d += 30)
      {
         double ang = d * M_PI / 180.0;
         float len  = 0.85f;

           std::vector<float> verts = {0.0f, 0.0f, 0.0f,
                                       static_cast<float>(len * std::sin(ang)),
                                       static_cast<float>(len * std::cos(ang)),
                                       0.0f};
          std::vector<float> cols  = {0.35f, 0.35f, 0.5f, 0.6f,
                                      0.35f, 0.35f, 0.5f, 0.6f};
         DrawLine(verts, cols);
      }
   }

   void DrawWindProfile()
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

      maxWindSpeed_ = 0.0;
      for (const auto& lvl : levels)
      {
         maxWindSpeed_ = std::max(maxWindSpeed_, lvl.wind_speed_mps_);
      }

      // Wind profile line (color-coded by height)
      std::vector<float> verts, cols;
      for (const auto& lvl : levels)
      {
          // Negate: meteorological wind direction (coming FROM) → hodograph vector (blowing TOWARD)
          double u = -lvl.wind_speed_mps_ *
                     std::sin(lvl.wind_direction_deg_ * M_PI / 180.0);
          double v = -lvl.wind_speed_mps_ *
                     std::cos(lvl.wind_direction_deg_ * M_PI / 180.0);

         float nx, ny;
         WindToNDC(u, v, nx, ny);
         verts.insert(verts.end(), {nx, ny, 0.0f});

          // Color by height: surface=warm (orange), tropopause=cool (purple)
          double heightKm = lvl.height_m_ / 1000.0;
          double t        = std::min(heightKm / 15.0, 1.0);
          float r = static_cast<float>(1.0 - t * 0.5);
          float g = static_cast<float>(0.5 + t * 0.2);
          float b = static_cast<float>(0.1 + t * 0.7);
         cols.insert(cols.end(), {r, g, b, 1.0f});
      }

      DrawLine(verts, cols);

      // Points at each level
      DrawPoints(verts, cols);
   }

   void DrawAxisLabels(QPainter& painter)
   {
      if (!sounding_) { return; }

      painter.setRenderHint(QPainter::Antialiasing);

      double maxWind = std::max(maxWindSpeed_, 10.0);
      int numRings   = static_cast<int>(std::ceil(maxWind / 10.0));

      painter.setPen(QColor(180, 180, 200));
      QFont labelFont = painter.font();
      labelFont.setPointSize(7);
      painter.setFont(labelFont);

      // Speed labels on right side
      for (int r = 1; r <= numRings; ++r)
      {
         double speed = r * 10.0;
         if (speed > maxWind * 1.2) { break; }

           float radius = static_cast<float>(speed / (maxWind * 1.2) * 0.85);

           // Position label at top of each ring (90 degrees)
          QPointF labelPos((radius + 1.0) / 2.0 * rect_.width() + 3,
                           (1.0 - radius - 0.02f) / 2.0 * rect_.height());
          painter.drawText(labelPos, QString::number(static_cast<int>(speed)));
      }

      // Cardinal direction labels
      painter.setPen(QColor(180, 180, 200));
      auto drawDirLabel = [&](const char* text, double deg,
                              double offsetX, double offsetY)
      {
         double ang = deg * M_PI / 180.0;
         double len = 0.92;
         QPointF pos(
             (len * std::sin(ang) + 1.0) / 2.0 * rect_.width() + offsetX,
             (1.0 - len * std::cos(ang)) / 2.0 * rect_.height() + offsetY);
         painter.drawText(pos, text);
      };

      drawDirLabel("N", 0, -5, 5);
      drawDirLabel("E", 90, -5, 5);
      drawDirLabel("S", 180, -5, 5);
      drawDirLabel("W", 270, -5, 5);
   }

   void Render()
   {
      if (!shaderLoaded_) { return; }

      glClearColor(0.08f, 0.08f, 0.15f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      projMatrix_ = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      DrawGrid();
      DrawWindProfile();

      glDisable(GL_BLEND);
   }

   HodographWidget*                              widget_;
   std::shared_ptr<gl::ShaderProgram>            shader_;
   glm::mat4                                     projMatrix_ {};
   std::shared_ptr<sounding::SoundingData>       sounding_;
   QRect                                         rect_ {};
   double                                        maxWindSpeed_ {10.0};
   bool                                          shaderLoaded_ {false};
};

HodographWidget::HodographWidget(QWidget* parent) :
    QOpenGLWidget(parent),
    p(std::make_unique<Impl>(this))
{
   setMinimumSize(300, 300);
}
HodographWidget::~HodographWidget() = default;

void HodographWidget::SetSounding(std::shared_ptr<sounding::SoundingData> sounding)
{
   p->sounding_ = sounding;
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

   QPainter painter(this);
   p->DrawAxisLabels(painter);
   painter.end();
}

} // namespace scwx::qt::view
