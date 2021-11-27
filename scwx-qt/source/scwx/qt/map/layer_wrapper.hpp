#pragma once

#include <scwx/qt/map/generic_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class LayerWrapperImpl;

class LayerWrapper : public QMapbox::CustomLayerHostInterface
{
public:
   explicit LayerWrapper(std::shared_ptr<GenericLayer> layer);
   ~LayerWrapper();

   LayerWrapper(const LayerWrapper&) = delete;
   LayerWrapper& operator=(const LayerWrapper&) = delete;

   LayerWrapper(LayerWrapper&&) noexcept;
   LayerWrapper& operator=(LayerWrapper&&) noexcept;

   void initialize() override final;
   void render(const QMapbox::CustomLayerRenderParameters&) override final;
   void deinitialize() override final;

private:
   std::unique_ptr<LayerWrapperImpl> p;
};

} // namespace map
} // namespace qt
} // namespace scwx
