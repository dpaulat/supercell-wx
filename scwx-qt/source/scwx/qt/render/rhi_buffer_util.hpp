#pragma once

#include <cstddef>

#include <rhi/qrhi.h>

namespace scwx::qt::render
{

bool EnsureDynamicBuffer(QRhi*            rhi,
                         QRhiBuffer*&     buffer,
                         std::size_t&     capacity,
                         QRhiBuffer::Type type,
                         QRhiBuffer::UsageFlags usage,
                         std::size_t      requiredBytes);

} // namespace scwx::qt::render
