#pragma once

#include <memory>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

class DrawItemImpl;

class DrawItem
{
public:
   explicit DrawItem();
   ~DrawItem();

   DrawItem(const DrawItem&) = delete;
   DrawItem& operator=(const DrawItem&) = delete;

   DrawItem(DrawItem&&) noexcept;
   DrawItem& operator=(DrawItem&&) noexcept;

   virtual void Initialize()   = 0;
   virtual void Render()       = 0;
   virtual void Deinitialize() = 0;

private:
   std::unique_ptr<DrawItemImpl> p;
};

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
