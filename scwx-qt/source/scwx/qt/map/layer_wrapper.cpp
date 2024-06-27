#include <scwx/qt/map/layer_wrapper.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class LayerWrapperImpl
{
public:
   explicit LayerWrapperImpl(std::shared_ptr<GenericLayer> layer) :
       layer_ {layer}
   {
   }

   ~LayerWrapperImpl() {}

   std::shared_ptr<GenericLayer> layer_;
};

LayerWrapper::LayerWrapper(std::shared_ptr<GenericLayer> layer) :
    p(std::make_unique<LayerWrapperImpl>(layer))
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
      layer->Initialize();
   }
}

void LayerWrapper::render(const QMapLibre::CustomLayerRenderParameters& params)
{
   auto& layer = p->layer_;
   if (layer != nullptr)
   {
      layer->Render(params);
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

} // namespace map
} // namespace qt
} // namespace scwx
