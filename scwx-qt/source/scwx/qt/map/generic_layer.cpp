#include <scwx/qt/map/generic_layer.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class GenericLayerImpl
{
public:
   explicit GenericLayerImpl() {}

   ~GenericLayerImpl() {}
};

GenericLayer::GenericLayer() : p(std::make_unique<GenericLayerImpl>()) {}
GenericLayer::~GenericLayer() = default;

} // namespace map
} // namespace qt
} // namespace scwx
