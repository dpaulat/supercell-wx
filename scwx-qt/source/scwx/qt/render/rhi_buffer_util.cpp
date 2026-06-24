#include <scwx/qt/render/rhi_buffer_util.hpp>

#include <algorithm>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

bool EnsureDynamicBuffer(QRhi*                  rhi,
                         QRhiBuffer*&           buffer,
                         std::size_t&           capacity,
                         QRhiBuffer::Type       type,
                         QRhiBuffer::UsageFlags usage,
                         std::size_t            requiredBytes)
{
   if (capacity >= requiredBytes)
   {
      return buffer != nullptr;
   }

   if (buffer != nullptr && buffer->type() != QRhiBuffer::Immutable)
   {
      buffer->destroy();
   }

   const std::size_t newCapacity =
      std::max(requiredBytes, capacity + capacity / 2 + 4096);
   capacity = newCapacity;
   if (buffer == nullptr)
   {
      buffer = rhi->newBuffer(type, usage, static_cast<quint32>(capacity));
      if (buffer == nullptr)
      {
         capacity = 0;
         return false;
      }
   }
   else
   {
      buffer->setSize(static_cast<quint32>(capacity));
   }
   if (!buffer->create())
   {
      capacity = 0;
      return false;
   }

   return true;
}

} // namespace scwx::qt::render
