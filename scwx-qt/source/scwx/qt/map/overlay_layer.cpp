// Enable chrono formatters
#ifndef __cpp_lib_format
#   define __cpp_lib_format 202110L
#endif

#include <scwx/qt/map/overlay_layer.hpp>
#include <scwx/qt/gl/draw/rectangle.hpp>
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
   explicit OverlayLayerImpl(std::shared_ptr<MapContext> context) :
       textShader_(context->gl_),
       font_(util::Font::Create(":/res/fonts/din1451alt.ttf")),
       texture_ {GL_INVALID_INDEX},
       activeBoxOuter_ {std::make_shared<gl::draw::Rectangle>(context->gl_)},
       activeBoxInner_ {std::make_shared<gl::draw::Rectangle>(context->gl_)},
       timeBox_ {std::make_shared<gl::draw::Rectangle>(context->gl_)},
       sweepTimeString_ {},
       sweepTimeNeedsUpdate_ {true}
   {
      // TODO: Manage font at the global level, texture at the view level
   }
   ~OverlayLayerImpl() = default;

   gl::TextShader              textShader_;
   std::shared_ptr<util::Font> font_;
   GLuint                      texture_;

   std::shared_ptr<gl::draw::Rectangle> activeBoxOuter_;
   std::shared_ptr<gl::draw::Rectangle> activeBoxInner_;
   std::shared_ptr<gl::draw::Rectangle> timeBox_;

   std::string sweepTimeString_;
   bool        sweepTimeNeedsUpdate_;
};

OverlayLayer::OverlayLayer(std::shared_ptr<MapContext> context) :
    DrawLayer(context), p(std::make_unique<OverlayLayerImpl>(context))
{
   AddDrawItem(p->timeBox_);
   AddDrawItem(p->activeBoxOuter_);
   AddDrawItem(p->activeBoxInner_);

   p->activeBoxOuter_->SetPosition(0.0f, 0.0f);
   p->activeBoxOuter_->SetBorder(1.0f, {0, 0, 0, 255});
   p->activeBoxInner_->SetBorder(1.0f, {255, 255, 255, 255});
   p->activeBoxInner_->SetPosition(1.0f, 1.0f);
   p->timeBox_->SetFill({0, 0, 0, 192});
}

OverlayLayer::~OverlayLayer() = default;

void OverlayLayer::Initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Initialize()";

   DrawLayer::Initialize();

   gl::OpenGLFunctions& gl = context()->gl_;

   p->textShader_.Initialize();

   if (p->texture_ == GL_INVALID_INDEX)
   {
      p->texture_ = p->font_->GenerateTexture(gl);
   }

   connect(context()->radarProductView_.get(),
           &view::RadarProductView::SweepComputed,
           this,
           &OverlayLayer::UpdateSweepTimeNextFrame);
}

void OverlayLayer::Render(const QMapbox::CustomLayerRenderParameters& params)
{
   constexpr float fontSize = 16.0f;

   gl::OpenGLFunctions& gl = context()->gl_;

   if (p->sweepTimeNeedsUpdate_)
   {
      using namespace std::chrono;
      auto sweepTime =
         time_point_cast<seconds>(context()->radarProductView_->sweep_time());

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

   // Active Box
   p->activeBoxOuter_->SetVisible(context()->settings_.isActive_);
   p->activeBoxInner_->SetVisible(context()->settings_.isActive_);
   if (context()->settings_.isActive_)
   {
      p->activeBoxOuter_->SetSize(params.width, params.height);
      p->activeBoxInner_->SetSize(params.width - 2.0f, params.height - 2.0f);
   }

   if (p->sweepTimeString_.length() > 0)
   {
      const float textLength =
         p->font_->TextLength(p->sweepTimeString_, fontSize);

      p->timeBox_->SetPosition(static_cast<float>(params.width) - textLength -
                                  14.0f,
                               static_cast<float>(params.height) - 22.0f);
      p->timeBox_->SetSize(textLength + 14.0f, 22.0f);
   }

   DrawLayer::Render(params);

   if (p->sweepTimeString_.length() > 0)
   {
      // Render time
      p->textShader_.RenderText(p->sweepTimeString_,
                                params.width - 7.0f,
                                static_cast<float>(params.height) - 16.0f,
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
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Deinitialize()";

   DrawLayer::Deinitialize();

   gl::OpenGLFunctions& gl = context()->gl_;

   gl.glDeleteTextures(1, &p->texture_);

   p->texture_ = GL_INVALID_INDEX;

   disconnect(context()->radarProductView_.get(),
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
