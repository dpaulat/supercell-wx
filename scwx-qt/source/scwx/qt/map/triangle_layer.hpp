#include <qmapboxgl.hpp>

namespace scwx
{
namespace qt
{

class TriangleLayerImpl;

class TriangleLayer : public QMapbox::CustomLayerHostInterface
{
public:
   explicit TriangleLayer();
   ~TriangleLayer();

   TriangleLayer(const TriangleLayer&) = delete;
   TriangleLayer& operator=(const TriangleLayer&) = delete;

   TriangleLayer(TriangleLayer&&) noexcept;
   TriangleLayer& operator=(TriangleLayer&&) noexcept;

   void initialize() override final;
   void render(const QMapbox::CustomLayerRenderParameters&) override final;
   void deinitialize() override final;

private:
   std::unique_ptr<TriangleLayerImpl> p;
};

} // namespace qt
} // namespace scwx
