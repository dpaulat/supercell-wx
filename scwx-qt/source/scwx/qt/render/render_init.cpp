#include <scwx/qt/render/render_backend.hpp>
#include <scwx/qt/render/render_init.hpp>
#include <scwx/util/logger.hpp>

namespace scwx::qt::render
{

static const std::string logPrefix_ = "scwx::qt::render::render_init";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

void InitializeGraphics()
{
   logger_->info("Render backend: {}", RenderBackendName(kRenderBackend));
}

} // namespace scwx::qt::render
