#pragma once

#include <QByteArray>

#include <rhi/qshader.h>

namespace scwx::qt::render
{

// Load a Qt Shader Baker pack (.qsb) that embeds SPIR-V (Vulkan) and MSL
// (Metal). QRhi selects the backend variant at pipeline creation.
[[nodiscard]] QShader LoadShader(const char* resourcePath);

} // namespace scwx::qt::render
