#include <scwx/util/logger.hpp>

#include <mutex>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace scwx
{
namespace util
{
namespace Logger
{

void Initialize()
{
   spdlog::set_pattern("[%Y-%m-%d %T.%e] [%t] [%^%l%$] [%n] %v");
}

std::shared_ptr<spdlog::logger> Create(const std::string& name)
{
   // Create a shared sink
   static auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

   // Create the logger
   std::shared_ptr<spdlog::logger> logger =
      std::make_shared<spdlog::logger>(name, sink);

   // Register the logger, so it can be retrieved later using spdlog::get()
   spdlog::register_logger(logger);

   return logger;
}

} // namespace Logger
} // namespace util
} // namespace scwx
