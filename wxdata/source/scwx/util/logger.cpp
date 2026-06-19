#include <scwx/util/logger.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace scwx::util::Logger
{

static const std::string logPattern_ = "[%Y-%m-%d %T.%e] [%t] [%^%l%$] [%n] %v";

static std::vector<std::shared_ptr<spdlog::sinks::sink>> extraSinks_ {};

static void AddDefaultSink();

void Initialize()
{
   spdlog::set_pattern(logPattern_);

   // Periodically flush every 3 seconds
   spdlog::flush_every(std::chrono::seconds(3));

   // Flush whenever logging info or higher
   spdlog::flush_on(spdlog::level::level_enum::info);

   AddDefaultSink();
}

void AddDefaultSink()
{
   const auto defaultSink =
      std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
   defaultSink->set_pattern(logPattern_);

   spdlog::apply_all(
      [&](const std::shared_ptr<spdlog::logger>& logger)
      {
         auto& sinks = logger->sinks();
         sinks.push_back(defaultSink);
      });

   extraSinks_.push_back(defaultSink);
}

void AddFileSink(const std::string& baseFilename)
{
   constexpr std::size_t maxSize      = 20u * 1024u * 1024u; // 20 MB
   constexpr std::size_t maxFiles     = 5u;
   constexpr bool        rotateOnOpen = true;

   auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      baseFilename, maxSize, maxFiles, rotateOnOpen);

   fileSink->set_pattern(logPattern_);

   spdlog::apply_all(
      [&](const std::shared_ptr<spdlog::logger>& logger)
      {
         auto& sinks = logger->sinks();
         sinks.push_back(fileSink);
      });

   extraSinks_.push_back(fileSink);
}

std::shared_ptr<spdlog::logger> Create(const std::string& name)
{
   // Create the empty logger
   std::shared_ptr<spdlog::logger> logger =
      std::make_shared<spdlog::logger>(name);

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

} // namespace scwx::util::Logger
