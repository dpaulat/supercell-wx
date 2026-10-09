#include <scwx/qt/util/polygon_triangulation.hpp>

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include <mapbox/earcut.hpp>
#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

namespace scwx::qt::util
{

template<typename Index>
std::vector<Index> TriangulatePolygon(const std::vector<PolygonRing2D>& rings)
{
   return mapbox::earcut<Index>(rings);
}

template std::vector<std::uint32_t>
TriangulatePolygon<std::uint32_t>(const std::vector<PolygonRing2D>&);

} // namespace scwx::qt::util
