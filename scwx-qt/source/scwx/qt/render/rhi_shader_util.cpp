#include <scwx/qt/render/rhi_shader_util.hpp>

#include <QFile>

namespace scwx::qt::render
{

QShader LoadSpirvShader(const char* resourcePath, QShader::Stage stage)
{
   QFile file {resourcePath};
   if (!file.open(QIODevice::ReadOnly))
   {
      return {};
   }

   QShader shader;
   shader.setStage(stage);

   QShaderKey  key {QShader::SpirvShader, QShaderVersion(100)};
   QShaderCode code {file.readAll(), QByteArrayLiteral("main")};
   shader.setShader(key, code);

   return shader;
}

} // namespace scwx::qt::render
