#include <scwx/util/logger.hpp>

#include <mutex>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace scwx
{
namespace util
{
namespace Logger
{

static std::vector<std::shared_ptr<spdlog::sinks::sink>> extraSinks_ {};

void Initialize()
{
   spdlog::set_pattern("[%Y-%m-%d %T.%e] [%t] [%^%l%$] [%n] %v");
}

void AddFileSink(const std::string& baseFilename)
{
   constexpr std::size_t maxSize      = 20u * 1024u * 1024u; // 20 MB
   constexpr std::size_t maxFiles     = 5u;
   constexpr bool        rotateOnOpen = true;

   auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      baseFilename, maxSize, maxFiles, rotateOnOpen);

   spdlog::apply_all(
      [&](std::shared_ptr<spdlog::logger> logger)
      {
         auto& sinks = logger->sinks();
         sinks.push_back(fileSink);
      });

   extraSinks_.push_back(fileSink);
}

std::shared_ptr<spdlog::logger> Create(const std::string& name)
{
   // Create a shared sink
   static auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

   // Create the logger
   std::shared_ptr<spdlog::logger> logger =
      std::make_shared<spdlog::logger>(name, sink);

   // Add additional registered sinks
   for (auto& extraSink : extraSinks_)
   {
      auto& sinks = logger->sinks();
      sinks.push_back(extraSink);
   }

   // Register the logger, so it can be retrieved later using spdlog::get()
   spdlog::register_logger(logger);

   return logger;
}

} // namespace Logger
} // namespace util
} // namespace scwx
