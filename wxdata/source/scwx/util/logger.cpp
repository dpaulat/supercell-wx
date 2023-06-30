#include <scwx/util/logger.hpp>

#include <mutex>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

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
   // Create stdout sink
   static auto stdoutSink =
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

   // Create file sink
   static auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
         "supercell-wx.log", 1048576, 1, false);

   // Create the logger
   std::vector<spdlog::sink_ptr>   sinks = {stdoutSink, fileSink};
   std::shared_ptr<spdlog::logger> logger =
      std::make_shared<spdlog::logger>(name, begin(sinks), end(sinks));

   // Register the logger, so it can be retrieved later using spdlog::get()
   spdlog::register_logger(logger);

   return logger;
}

} // namespace Logger
} // namespace util
} // namespace scwx
