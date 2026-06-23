#pragma once

#include <QByteArray>

#include <rhi/qshader.h>

namespace scwx::qt::render
{

[[nodiscard]] QShader LoadSpirvShader(const char*    resourcePath,
                                      QShader::Stage stage);

} // namespace scwx::qt::render
