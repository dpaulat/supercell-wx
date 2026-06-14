#include <scwx/qt/map/layer_wrapper.hpp>
#include <scwx/qt/render/render_backend.hpp>

namespace scwx::qt::map
{

class LayerWrapper::Impl
{
public:
   explicit Impl(std::shared_ptr<GenericLayer> layer,
                 std::shared_ptr<MapContext>   mapContext) :
       layer_ {std::move(layer)}, mapContext_ {std::move(mapContext)}
   {
   }

   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::shared_ptr<GenericLayer> layer_;
   std::shared_ptr<MapContext>   mapContext_;
};

LayerWrapper::LayerWrapper(std::shared_ptr<GenericLayer> layer,
                           std::shared_ptr<MapContext>   mapContext) :
    p(std::make_unique<Impl>(std::move(layer), std::move(mapContext)))
{
}
LayerWrapper::~LayerWrapper() = default;

LayerWrapper::LayerWrapper(LayerWrapper&&) noexcept            = default;
LayerWrapper& LayerWrapper::operator=(LayerWrapper&&) noexcept = default;

void LayerWrapper::initialize()
{
   auto& layer = p->layer_;
   if (layer != nullptr)
   {
      layer->Initialize(p->mapContext_);
   }
}

void LayerWrapper::render(const QMapLibre::CustomLayerRenderParameters& params)
{
#if defined(SCWX_RENDER_BACKEND_VULKAN)
   Q_UNUSED(params);
   return;
#endif

   auto& layer = p->layer_;
   if (layer != nullptr)
   {
      layer->Render(p->mapContext_, params);
   }
}

void LayerWrapper::deinitialize()
{
   // Ensure layers are not retained after call to deinitialize
   auto& layer = p->layer_;
   if (layer != nullptr)
   {
      layer->Deinitialize();
      layer = nullptr;
   }
}

} // namespace scwx::qt::map
