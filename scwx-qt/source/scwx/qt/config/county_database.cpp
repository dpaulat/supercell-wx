#include <scwx/qt/config/county_database.hpp>
#include <scwx/util/logger.hpp>

#include <shared_mutex>
#include <unordered_map>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <QFile>
#include <QStandardPaths>
#include <sqlite3.h>

namespace scwx
{
namespace qt
{
namespace config
{
namespace CountyDatabase
{

static const std::string logPrefix_ = "scwx::qt::config::county_database";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string countyDatabaseFilename_ = ":/res/db/counties.db";

static bool                                         initialized_ {false};
static std::unordered_map<std::string, std::string> countyMap_;
static std::shared_mutex                            countyMutex_;

void Initialize()
{
   if (initialized_)
   {
      return;
   }

   logger_->debug("Loading database");

   // Generate UUID for temporary file
   boost::uuids::uuid uuid = boost::uuids::random_generator()();

   std::string tempPath {
      QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         .toStdString()};
   std::string countyDatabaseCache {tempPath + "/scwx-" +
                                    boost::uuids::to_string(uuid)};

   // Create cache directory if it doesn't exist
   if (!std::filesystem::exists(tempPath))
   {
      if (!std::filesystem::create_directories(tempPath))
      {
         logger_->error("Unable to create temp directory: \"{}\"", tempPath);
         return;
      }
   }

   // Remove existing county database if it exists
   if (std::filesystem::exists(countyDatabaseCache))
   {
      std::filesystem::remove(countyDatabaseCache);
   }

   // Create a fresh copy of the county database in the temporary directory
   QFile countyDatabaseFile(QString::fromStdString(countyDatabaseFilename_));
   if (!countyDatabaseFile.copy(QString::fromStdString(countyDatabaseCache)))
   {
      logger_->error("Unable to create cached copy of database: \"{}\" ({})",
                     countyDatabaseCache,
                     countyDatabaseFile.errorString().toStdString());
      return;
   }

   // Open database
   sqlite3* db;
   int      rc;
   char*    errorMessage = nullptr;

   rc = sqlite3_open(countyDatabaseCache.c_str(), &db);
   if (rc != SQLITE_OK)
   {
      logger_->error("Unable to open database: \"{}\"", countyDatabaseCache);
      sqlite3_close(db);
      std::filesystem::remove(countyDatabaseCache);
      return;
   }

   // Database is open, acquire lock
   std::unique_lock lock(countyMutex_);

   // Query database for counties
   rc = sqlite3_exec(
      db,
      "SELECT * FROM counties",
      [](void* /* param */,
         int    columns,
         char** columnText,
         char** /* columnName */) -> int
      {
         int status = 0;

         if (columns == 2)
         {
            countyMap_.emplace(columnText[0], columnText[1]);
         }
         else
         {
            logger_->error(
               "Database format error, invalid number of columns: {}", columns);
            status = -1;
         }

         return status;
      },
      nullptr,
      &errorMessage);
   if (rc != SQLITE_OK)
   {
      logger_->error("SQL error: {}", errorMessage);
      sqlite3_free(errorMessage);
   }

   // Finished populating county map, release lock
   lock.unlock();

   // Close database
   sqlite3_close(db);

   // Remove temporary file
   std::error_code err;

   if (!std::filesystem::remove(countyDatabaseCache, err))
   {
      logger_->warn("Unable to remove cached copy of database, error: {}",
                    err.message());
   }

   initialized_ = true;
}

std::string GetCountyName(const std::string& id)
{
   std::shared_lock lock(countyMutex_);

   auto it = countyMap_.find(id);
   if (it != countyMap_.cend())
   {
      return it->second;
   }

   return id;
}

} // namespace CountyDatabase
} // namespace config
} // namespace qt
} // namespace scwx
