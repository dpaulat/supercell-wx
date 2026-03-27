#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/main/program_options.hpp>
#include <scwx/util/logger.hpp>

#include <string>
#include <vector>

#include <boost/unordered/unordered_flat_map.hpp>
#include <QStandardPaths>

namespace scwx::qt::main::ApplicationPaths
{
static const std::string logPrefix_ = "scwx::qt::main::application_paths";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const boost::unordered_flat_map<StandardLocation, std::string>
   standardLocationNames_ {{StandardLocation::Cache, "Cache"},
                           {StandardLocation::FontCache, "Font Cache"},
                           {StandardLocation::Local, "Local"},
                           {StandardLocation::Log, "Log"},
                           {StandardLocation::Pictures, "Pictures"},
                           {StandardLocation::Settings, "Settings"},
                           {StandardLocation::Temp, "Temp"}};

static const boost::unordered_flat_map<
   StandardLocation,
   std::pair<QStandardPaths::StandardLocation, std::string>>
   standardLocationDefaultPaths_ {
      {StandardLocation::Cache, {QStandardPaths::CacheLocation, ""}},
      {StandardLocation::FontCache, {QStandardPaths::CacheLocation, "/fonts"}},
      {StandardLocation::Local, {QStandardPaths::AppLocalDataLocation, ""}},
      {StandardLocation::Log, {QStandardPaths::AppLocalDataLocation, ""}},
      {StandardLocation::Pictures, {QStandardPaths::PicturesLocation, ""}},
      {StandardLocation::Settings, {QStandardPaths::AppDataLocation, ""}},
      {StandardLocation::Temp, {QStandardPaths::TempLocation, ""}}};

static const boost::unordered_flat_map<StandardLocation, std::string>
   standardLocationPortablePaths_ {
      {StandardLocation::Cache, "cache/"},
      {StandardLocation::FontCache, "cache/fonts/"},
      {StandardLocation::Local, "local/"},
      {StandardLocation::Log, "logs/"},
      {StandardLocation::Pictures, "pictures/"},
      {StandardLocation::Settings, "settings/"},
      {StandardLocation::Temp, "temp/"}};

static boost::unordered_flat_map<StandardLocation, std::filesystem::path>
   standardLocationPaths_ {};

static std::vector<std::string> errorMessages_ {};

static bool                  InitializeOverride(StandardLocation location);
static std::filesystem::path GetDefaultPath(StandardLocation location);

void Initialize()
{
   for (const auto location : StandardLocationIterator())
   {
      if (InitializeOverride(location))
      {
         continue;
      }

      const std::filesystem::path path = GetDefaultPath(location);

      if (!std::filesystem::exists(path))
      {
         std::error_code error;
         if (!std::filesystem::create_directories(path, error))
         {
            const std::string errorMessage =
               fmt::format("Unable to create {} directory: \"{}\" ({})",
                           standardLocationNames_.at(location),
                           path.string(),
                           error.message());
            errorMessages_.push_back(errorMessage);
         }
      }

      standardLocationPaths_.emplace(location, path);
   }
}

bool InitializeOverride(StandardLocation location)
{
   std::string overridePath {};

   switch (location)
   {
   case StandardLocation::Settings:
      overridePath = ProgramOptions::GetOptions().settingsDirectory_;
      break;

   case StandardLocation::Cache:
   case StandardLocation::FontCache:
   case StandardLocation::Log:
   case StandardLocation::Pictures:
   case StandardLocation::Temp:
      break;
   }

   if (!overridePath.empty())
   {
      const std::filesystem::path path {overridePath};

      if (!std::filesystem::exists(path))
      {
         std::error_code error;
         if (!std::filesystem::create_directories(path, error))
         {
            const std::string errorMessage =
               fmt::format("Unable to create {} directory: \"{}\" ({})",
                           standardLocationNames_.at(location),
                           path.string(),
                           error.message());
            errorMessages_.push_back(errorMessage);
            return false;
         }
      }

      if (std::filesystem::exists(path))
      {
         standardLocationPaths_.emplace(location, path);
         return true;
      }
   }

   return false;
}

std::filesystem::path GetDefaultPath(StandardLocation location)
{
   if (ProgramOptions::GetOptions().portableMode_)
   {
      return standardLocationPortablePaths_.at(location);
   }
   else
   {
      const auto& [qtLocation, subPath] =
         standardLocationDefaultPaths_.at(location);
      return QStandardPaths::writableLocation(qtLocation).toStdString() +
             subPath;
   }
}

void LogErrors()
{
   for (const auto& errorMessage : errorMessages_)
   {
      logger_->error(errorMessage);
   }
}

[[nodiscard]] const std::filesystem::path& GetLocation(StandardLocation type)
{
   return standardLocationPaths_.at(type);
}

} // namespace scwx::qt::main::ApplicationPaths
