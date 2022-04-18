#include <scwx/qt/gl/draw/draw_item.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::draw_item";

class DrawItemImpl
{
public:
   explicit DrawItemImpl() {}

   ~DrawItemImpl() {}
};

DrawItem::DrawItem() : p(std::make_unique<DrawItemImpl>()) {}
DrawItem::~DrawItem() = default;

DrawItem::DrawItem(DrawItem&&) noexcept = default;
DrawItem& DrawItem::operator=(DrawItem&&) noexcept = default;

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
