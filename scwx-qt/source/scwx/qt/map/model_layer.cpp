#include <scwx/qt/map/model_layer.hpp>

#include <scwx/qt/gl/gl.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/qt/manager/model_manager.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

#include <array>
#include <cmath>
#include <mutex>

#include <QImage>
#include <QPainter>
#include <glm/gtc/type_ptr.hpp>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::model_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ModelLayer::Impl
{
public:
   std::shared_ptr<gl::ShaderProgram>     shader_;
   std::shared_ptr<manager::ModelManager> manager_ {
      manager::ModelManager::Instance()};
   GLuint            vao_ {GL_INVALID_INDEX};
   GLuint            vbo_ {GL_INVALID_INDEX};
   GLuint            texture_ {GL_INVALID_INDEX};
   GLint             mapMatrixLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint             originLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint             opacityLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   std::mutex        mutex_;
   QImage            image_;
   types::ModelFrame frame_;
   bool              dirty_ {false};
   bool              visible_ {true};
   float             opacity_ {0.8f};
};

ModelLayer::ModelLayer(std::shared_ptr<gl::GlContext> glContext) :
    GenericLayer(std::move(glContext)), p(std::make_unique<Impl>())
{
   p->visible_ = p->manager_->visible();
   p->opacity_ = p->manager_->opacity();
   connect(p->manager_.get(),
           &manager::ModelManager::FramesSelected,
           this,
           [this](const QVector<types::ModelFrame>& frames)
           {
              if (frames.isEmpty())
              {
                 {
                    std::scoped_lock lock {p->mutex_};
                    p->image_ = {};
                    p->dirty_ = false;
                 }
                 Q_EMIT NeedsRendering();
                 return;
              }
              QImage composite(frames.front().width_,
                               frames.front().height_,
                               QImage::Format_RGBA8888);
              composite.fill(Qt::transparent);
              QPainter painter(&composite);
              bool     drewFrame = false;
              for (const auto& frame : frames)
              {
                 QImage image(frame.path_);
                 if (image.isNull())
                 {
                    logger_->warn("Could not load model overlay {}",
                                  frame.path_.toStdString());
                    continue;
                 }
                 if (image.size() != composite.size())
                 {
                    logger_->warn(
                       "Ignoring model overlay with mismatched size: {}",
                       frame.path_.toStdString());
                    continue;
                 }
                 painter.drawImage(0, 0, image);
                 drewFrame = true;
              }
              painter.end();
              if (!drewFrame)
              {
                 {
                    std::scoped_lock lock {p->mutex_};
                    p->image_ = {};
                    p->dirty_ = false;
                 }
                 Q_EMIT NeedsRendering();
                 return;
              }
              {
                 std::scoped_lock lock {p->mutex_};
                 p->image_ = std::move(composite);
                 p->frame_ = frames.front();
                 p->dirty_ = true;
              }
              Q_EMIT NeedsRendering();
           });
   connect(p->manager_.get(),
           &manager::ModelManager::OpacityChanged,
           this,
           [this](float opacity)
           {
              {
                 std::scoped_lock lock {p->mutex_};
                 p->opacity_ = opacity;
              }
              Q_EMIT NeedsRendering();
           });
   connect(p->manager_.get(),
           &manager::ModelManager::VisibilityChanged,
           this,
           [this](bool visible)
           {
              {
                 std::scoped_lock lock {p->mutex_};
                 p->visible_ = visible;
              }
              Q_EMIT NeedsRendering();
           });
}

ModelLayer::~ModelLayer() = default;

void ModelLayer::Initialize(const std::shared_ptr<MapContext>&)
{
   p->shader_ = gl_context()->GetShaderProgram(":/gl/model_overlay.vert",
                                               ":/gl/model_overlay.frag");
   p->mapMatrixLocation_ = p->shader_->GetUniformLocation("uMapMatrix");
   p->originLocation_    = p->shader_->GetUniformLocation("uOriginLatLong");
   p->opacityLocation_   = p->shader_->GetUniformLocation("uOpacity");

   glGenVertexArrays(1, &p->vao_);
   glGenBuffers(1, &p->vbo_);
   glGenTextures(1, &p->texture_);
   glBindTexture(GL_TEXTURE_2D, p->texture_);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   glBindVertexArray(p->vao_);
   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   glVertexAttribPointer(
      0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast<void*>(nullptr));
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(1,
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         4 * sizeof(float),
                         reinterpret_cast<void*>(2 * sizeof(float)));
   glEnableVertexAttribArray(1);
}

void ModelLayer::Render(const std::shared_ptr<MapContext>&,
                        const QMapLibre::CustomLayerRenderParameters& params)
{
   std::scoped_lock lock {p->mutex_};
   if (!p->visible_ || p->image_.isNull())
      return;

   if (p->dirty_)
   {
      const auto&  f = p->frame_;
      const double east =
         std::abs(f.east_ - f.west_) >= 359.0 ? f.west_ + 359.999 : f.east_;
      const std::array<float, 24> vertices {
         static_cast<float>(f.north_), static_cast<float>(f.west_), 0.0f, 0.0f,
         static_cast<float>(f.south_), static_cast<float>(f.west_), 0.0f, 1.0f,
         static_cast<float>(f.south_), static_cast<float>(east),    1.0f, 1.0f,
         static_cast<float>(f.north_), static_cast<float>(f.west_), 0.0f, 0.0f,
         static_cast<float>(f.south_), static_cast<float>(east),    1.0f, 1.0f,
         static_cast<float>(f.north_), static_cast<float>(east),    1.0f, 0.0f};
      glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                   vertices.data(),
                   GL_STATIC_DRAW);
      glBindTexture(GL_TEXTURE_2D, p->texture_);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   p->image_.width(),
                   p->image_.height(),
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   p->image_.constBits());
      p->dirty_ = false;
   }

   p->shader_->Use();
   const auto matrix = util::maplibre::GetMapMatrix(params);
   glUniformMatrix4fv(
      p->mapMatrixLocation_, 1, GL_FALSE, glm::value_ptr(matrix));
   glUniform2f(p->originLocation_, params.latitude, params.longitude);
   glUniform1f(p->opacityLocation_, p->opacity_);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, p->texture_);
   glBindVertexArray(p->vao_);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDrawArrays(GL_TRIANGLES, 0, 6);
   SCWX_GL_CHECK_ERROR();
}

void ModelLayer::Deinitialize()
{
   glDeleteTextures(1, &p->texture_);
   glDeleteBuffers(1, &p->vbo_);
   glDeleteVertexArrays(1, &p->vao_);
   p->texture_ = GL_INVALID_INDEX;
   p->vbo_     = GL_INVALID_INDEX;
   p->vao_     = GL_INVALID_INDEX;
}

} // namespace scwx::qt::map
