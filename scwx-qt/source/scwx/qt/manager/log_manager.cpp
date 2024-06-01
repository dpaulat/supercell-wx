#include <scwx/qt/manager/log_manager.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <map>
#include <ranges>
#include <unordered_map>

#include <boost/process/environment.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <QStandardPaths>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::log_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static void QtLogMessageHandler(QtMsgType                 messageType,
                                const QMessageLogContext& context,
                                const QString&            message);

class LogManager::Impl
{
public:
   explicit Impl() {}
   ~Impl() {}

   void PruneLogFiles();

   std::string logPath_ {};
   std::string logFile_ {};
   int         pid_ {};
};

LogManager::LogManager() : p(std::make_unique<Impl>()) {}
LogManager::~LogManager() = default;

void LogManager::Initialize()
{
   // Initialize logger
   scwx::util::Logger::Initialize();
   spdlog::set_level(spdlog::level::debug);

   // Install Qt Message Handler
   qInstallMessageHandler(&QtLogMessageHandler);
}

void LogManager::InitializeLogFile()
{
   p->logPath_ =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
         .toStdString();
   p->pid_     = boost::this_process::get_id();
   p->logFile_ = fmt::format("{}/supercell-wx.{}.log", p->logPath_, p->pid_);

   // Create log directory if it doesn't exist
   if (!std::filesystem::exists(p->logPath_))
   {
      if (!std::filesystem::create_directories(p->logPath_))
      {
         logger_->error("Unable to create log directory: \"{}\"", p->logPath_);
         return;
      }
   }

   scwx::util::Logger::AddFileSink(p->logFile_);

   p->PruneLogFiles();
}

void LogManager::Impl::PruneLogFiles()
{
   using namespace std::chrono_literals;

   static constexpr std::size_t kMaxLogFiles_        = 5;
   static constexpr auto        kMinModificationAge_ = 30min;

   std::multimap<std::filesystem::file_time_type, std::filesystem::path>
      logFiles {};

   // Find existing log files in log directory
   for (auto& file : std::filesystem::directory_iterator(logPath_))
   {
      const std::string filename = file.path().filename().string();

      if (file.is_regular_file() && filename.starts_with("supercell-wx.") &&
          filename.ends_with(".log"))
      {
         logger_->trace("Found log file: {}", filename);

         try
         {
            auto lastWriteTime = std::filesystem::last_write_time(file);
            logFiles.insert({lastWriteTime, file});
         }
         catch (const std::exception&)
         {
            logger_->error("Error getting last write time of file: {}",
                           file.path().string());
         }
      }
   }

   // Clean up old log files
   auto now                   = std::filesystem::file_time_type::clock::now();
   auto modificationThreshold = now - kMinModificationAge_;

   for (auto& logFile :
        logFiles | std::views::reverse | std::views::drop(kMaxLogFiles_))
   {
      if (logFile.first < modificationThreshold)
      {
         const std::string filename = logFile.second.filename().string();

         logger_->info("Removing old log file: {}", filename);

         std::error_code ec;
         if (!std::filesystem::remove(logFile.second, ec))
         {
            logger_->warn(
               "Could not remove file: {}, {}", filename, ec.message());
         }
      }
   }
}

LogManager& LogManager::Instance()
{
   static LogManager LogManager_ {};
   return LogManager_;
}

void QtLogMessageHandler(QtMsgType                 messageType,
                         const QMessageLogContext& context,
                         const QString&            message)
{
   static const auto qtLogger_ = scwx::util::Logger::Create("qt");

   static const std::unordered_map<QtMsgType, spdlog::level::level_enum>
      levelMap_ {{QtMsgType::QtDebugMsg, spdlog::level::level_enum::debug},
                 {QtMsgType::QtInfoMsg, spdlog::level::level_enum::info},
                 {QtMsgType::QtWarningMsg, spdlog::level::level_enum::warn},
                 {QtMsgType::QtCriticalMsg, spdlog::level::level_enum::err},
                 {QtMsgType::QtFatalMsg, spdlog::level::level_enum::critical}};

   spdlog::level::level_enum level = spdlog::level::level_enum::info;
   auto                      it    = levelMap_.find(messageType);
   if (it != levelMap_.cend())
   {
      level = it->second;
   }

   spdlog::source_loc location {};
   if (context.file != nullptr && context.function != nullptr)
   {
      location = {context.file, context.line, context.function};
   }

   if (context.category != nullptr)
   {
      qtLogger_->log(
         location, level, "[{}] {}", context.category, message.toStdString());
   }
   else
   {
      qtLogger_->log(location, level, message.toStdString());
   }
}

} // namespace manager
} // namespace qt
} // namespace scwx
