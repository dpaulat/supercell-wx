#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/qt/gl/text_shader.hpp>
#include <scwx/qt/util/font.hpp>

#include <chrono>
#include <execution>

#include <boost/date_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/timer/timer.hpp>
#include <GeographicLib/Geodesic.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mbgl/util/constants.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "[scwx::qt::map::overlay_layer] ";

class OverlayLayerImpl
{
public:
   explicit OverlayLayerImpl(
      std::shared_ptr<view::RadarProductView> radarProductView,
      gl::OpenGLFunctions&                    gl) :
       radarProductView_(radarProductView),
       gl_(gl),
       textShader_(gl),
       font_(util::Font::Create(":/res/fonts/din1451alt.ttf")),
       shaderProgram_(gl),
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uColorLocation_(GL_INVALID_INDEX),
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX},
       texture_ {GL_INVALID_INDEX},
       sweepTimeString_ {},
       sweepTimeNeedsUpdate_ {true}
   {
      // TODO: Manage font at the global level, texture at the view level
   }
   ~OverlayLayerImpl() = default;

   std::shared_ptr<view::RadarProductView> radarProductView_;
   gl::OpenGLFunctions&                    gl_;

   gl::TextShader              textShader_;
   std::shared_ptr<util::Font> font_;
   gl::ShaderProgram           shaderProgram_;
   GLint                       uMVPMatrixLocation_;
   GLint                       uColorLocation_;
   std::array<GLuint, 2>       vbo_;
   GLuint                      vao_;
   GLuint                      texture_;

   std::string sweepTimeString_;
   bool        sweepTimeNeedsUpdate_;
};

OverlayLayer::OverlayLayer(
   std::shared_ptr<view::RadarProductView> radarProductView,
   gl::OpenGLFunctions&                    gl) :
    p(std::make_unique<OverlayLayerImpl>(radarProductView, gl))
{
}
OverlayLayer::~OverlayLayer() = default;

void OverlayLayer::Initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "initialize()";

   gl::OpenGLFunctions& gl = p->gl_;

   p->textShader_.Initialize();

   // Load and configure overlay shader
   p->shaderProgram_.Load(":/gl/overlay.vert", ":/gl/overlay.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_.id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find uMVPMatrix";
   }

   p->uColorLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_.id(), "uColor");
   if (p->uColorLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find uColor";
   }

   if (p->texture_ == GL_INVALID_INDEX)
   {
      p->texture_ = p->font_->GenerateTexture(gl);
   }

   p->shaderProgram_.Use();

   // Generate a vertex array object
   gl.glGenVertexArrays(1, &p->vao_);

   // Generate vertex buffer objects
   gl.glGenBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   gl.glBindVertexArray(p->vao_);

   // Active box (dynamic sized)
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   gl.glBufferData(
      GL_ARRAY_BUFFER, sizeof(float) * 5 * 2, nullptr, GL_DYNAMIC_DRAW);

   gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // Upper right panel (dynamic sized)
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   gl.glBufferData(
      GL_ARRAY_BUFFER, sizeof(float) * 6 * 2, nullptr, GL_DYNAMIC_DRAW);

   gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // Upper right panel color
   gl.glUniform4f(p->uColorLocation_, 0.0f, 0.0f, 0.0f, 0.75f);

   connect(p->radarProductView_.get(),
           &view::RadarProductView::SweepComputed,
           this,
           &OverlayLayer::UpdateSweepTimeNextFrame);
}

void OverlayLayer::Render(const QMapbox::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = p->gl_;

   if (p->sweepTimeNeedsUpdate_)
   {
      using namespace std::chrono;
      auto sweepTime =
         time_point_cast<seconds>(p->radarProductView_->sweep_time());

      if (sweepTime.time_since_epoch().count() != 0)
      {
         zoned_time         zt = {current_zone(), sweepTime};
         std::ostringstream os;
         os << zt;
         p->sweepTimeString_ = os.str();
      }

      p->sweepTimeNeedsUpdate_ = false;
   }

   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   p->shaderProgram_.Use();

   gl.glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(projection));

   if (p->radarProductView_->IsActive())
   {
      const float vertexLX       = 1.0f;
      const float vertexRX       = static_cast<float>(params.width) - 1.0f;
      const float vertexTY       = static_cast<float>(params.height) - 1.0f;
      const float vertexBY       = 1.0f;
      const float vertices[5][2] = {{vertexLX, vertexTY},  // TL
                                    {vertexLX, vertexBY},  // BL
                                    {vertexRX, vertexBY},  // BR
                                    {vertexRX, vertexTY},  // TR
                                    {vertexLX, vertexTY}}; // TL

      // Draw vertices
      gl.glBindVertexArray(p->vao_);
      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
      gl.glVertexAttribPointer(
         0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
      gl.glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
      gl.glDrawArrays(GL_LINE_STRIP, 0, 5);
   }

   if (p->sweepTimeString_.length() > 0)
   {
      const float fontSize = 16.0f;
      const float textLength =
         p->font_->TextLength(p->sweepTimeString_, fontSize);

      // Upper right panel vertices
      const float vertexLX =
         static_cast<float>(params.width) - textLength - 14.0f;
      const float vertexRX       = static_cast<float>(params.width);
      const float vertexTY       = static_cast<float>(params.height);
      const float vertexBY       = static_cast<float>(params.height) - 22.0f;
      const float vertices[6][2] = {{vertexLX, vertexTY}, // TL
                                    {vertexLX, vertexBY}, // BL
                                    {vertexRX, vertexTY}, // TR
                                    //
                                    {vertexLX, vertexBY},  // BL
                                    {vertexRX, vertexTY},  // TR
                                    {vertexRX, vertexBY}}; // BR

      // Draw vertices
      gl.glBindVertexArray(p->vao_);
      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
      gl.glVertexAttribPointer(
         0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
      gl.glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
      gl.glDrawArrays(GL_TRIANGLES, 0, 6);

      // Render time
      p->textShader_.RenderText(p->sweepTimeString_,
                                params.width - 7.0f,
                                static_cast<float>(params.height) -
                                   16.0f, // 7.0f,
                                fontSize,
                                projection,
                                boost::gil::rgba8_pixel_t(255, 255, 255, 204),
                                p->font_,
                                p->texture_,
                                gl::TextAlign::Right);
   }

   SCWX_GL_CHECK_ERROR();
}

void OverlayLayer::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->gl_;

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "deinitialize()";

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());
   gl.glDeleteTextures(1, &p->texture_);

   p->uMVPMatrixLocation_ = GL_INVALID_INDEX;
   p->uColorLocation_     = GL_INVALID_INDEX;
   p->vao_                = GL_INVALID_INDEX;
   p->vbo_                = {GL_INVALID_INDEX};
   p->texture_            = GL_INVALID_INDEX;

   disconnect(p->radarProductView_.get(),
              &view::RadarProductView::SweepComputed,
              this,
              &OverlayLayer::UpdateSweepTimeNextFrame);
}

void OverlayLayer::UpdateSweepTimeNextFrame()
{
   p->sweepTimeNeedsUpdate_ = true;
}

} // namespace map
} // namespace qt
} // namespace scwx
