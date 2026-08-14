#include <scwx/qt/map/generic_layer.hpp>

#include <algorithm>

namespace scwx::qt::map
{

class GenericLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<gl::GlContext> glContext) :
       glContext_ {std::move(glContext)}
   {
   }

   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::shared_ptr<gl::GlContext> glContext_;
   float                          opacity_ {1.0f};
};

GenericLayer::GenericLayer(std::shared_ptr<gl::GlContext> glContext) :
    p(std::make_unique<Impl>(std::move(glContext)))
{
}
GenericLayer::~GenericLayer() = default;

bool GenericLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& /* mouseGlobalPos */,
   const glm::vec2& /* mousePos */,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   // By default, the layer has nothing to pick
   return false;
}

std::shared_ptr<gl::GlContext> GenericLayer::gl_context() const
{
   return p->glContext_;
}

void GenericLayer::set_opacity(float opacity)
{
   p->opacity_ = std::clamp(opacity, 0.0f, 1.0f);
}

float GenericLayer::opacity() const
{
   return p->opacity_;
}

} // namespace scwx::qt::map
