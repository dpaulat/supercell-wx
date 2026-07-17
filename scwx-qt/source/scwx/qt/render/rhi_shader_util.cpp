#include <scwx/qt/render/rhi_shader_util.hpp>
#include <scwx/util/logger.hpp>

#include <QFile>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::rhi_shader_util";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

QShader LoadShader(const char* resourcePath)
{
   QFile file {resourcePath};
   if (!file.open(QIODevice::ReadOnly))
   {
      logger_->error("Unable to open shader resource: {}", resourcePath);
      return {};
   }

   const QByteArray bytes = file.readAll();
   QShader          shader = QShader::fromSerialized(bytes);
   if (!shader.isValid())
   {
      logger_->error("Invalid .qsb shader: {}", resourcePath);
      return {};
   }

   return shader;
}

} // namespace scwx::qt::render
