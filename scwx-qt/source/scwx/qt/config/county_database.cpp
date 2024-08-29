#include <scwx/qt/config/county_database.hpp>
#include <scwx/util/logger.hpp>

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

typedef std::unordered_map<std::string, std::string> CountyMap;
typedef std::unordered_map<std::string, CountyMap>   StateMap;
typedef std::unordered_map<char, StateMap>           FormatMap;

static bool                                         initialized_ {false};
static FormatMap                                    countyDatabase_;
static std::unordered_map<std::string, std::string> stateMap_;
static std::unordered_map<std::string, std::string> wfoMap_;

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

   // Ensure counties exists
   countyDatabase_.emplace('C', StateMap {});

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

         if (columns == 2 && std::strlen(columnText[0]) == 6)
         {
            std::string fipsId = columnText[0];
            std::string state  = fipsId.substr(0, 2);
            char        type   = fipsId.at(2);

            countyDatabase_[type][state].emplace(fipsId, columnText[1]);
         }
         else if (columns != 2)
         {
            logger_->error(
               "County database format error, invalid number of columns: {}",
               columns);
            status = -1;
         }
         else
         {
            logger_->error("Invalid FIPS ID: {}", columnText[0]);
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

   // Query database for states
   rc = sqlite3_exec(
      db,
      "SELECT * FROM states",
      [](void* /* param */,
         int    columns,
         char** columnText,
         char** /* columnName */) -> int
      {
         int status = 0;

         if (columns == 2)
         {
            stateMap_.emplace(columnText[0], columnText[1]);
         }
         else
         {
            logger_->error(
               "State database format error, invalid number of columns: {}",
               columns);
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

   // Query database for WFOs
   rc = sqlite3_exec(
      db,
      "SELECT id, city_state FROM wfos",
      [](void* /* param */,
         int    columns,
         char** columnText,
         char** /* columnName */) -> int
      {
         int status = 0;

         if (columns == 2)
         {
            wfoMap_.emplace(columnText[0], columnText[1]);
         }
         else
         {
            logger_->error(
               "WFO database format error, invalid number of columns: {}",
               columns);
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

   // Close database
   sqlite3_close(db);

   // Remove temporary file
   std::error_code error;
   if (!std::filesystem::remove(countyDatabaseCache, error))
   {
      logger_->warn("Unable to remove cached copy of database: {}",
                    error.message());
   }

   initialized_ = true;
}

std::string GetCountyName(const std::string& id)
{
   if (id.length() > 3)
   {
      // SSFNNN
      char        format = id.at(2);
      std::string state  = id.substr(0, 2);

      auto stateIt = countyDatabase_.find(format);
      if (stateIt != countyDatabase_.cend())
      {
         StateMap& states   = stateIt->second;
         auto      countyIt = states.find(state);
         if (countyIt != states.cend())
         {
            CountyMap& counties = countyIt->second;
            auto       it       = counties.find(id);
            if (it != counties.cend())
            {
               return it->second;
            }
         }
      }
   }

   return id;
}

std::unordered_map<std::string, std::string>
GetCounties(const std::string& state)
{
   std::unordered_map<std::string, std::string> counties {};

   StateMap& states = countyDatabase_.at('C');
   auto      it     = states.find(state);
   if (it != states.cend())
   {
      counties = it->second;
   }

   return counties;
}

const std::unordered_map<std::string, std::string>& GetStates()
{
   return stateMap_;
}

const std::unordered_map<std::string, std::string>& GetWFOs()
{
   return wfoMap_;
}

const std::string& GetWFOName(const std::string& wfoId)
{
   return wfoMap_.at(wfoId);
}

} // namespace CountyDatabase
} // namespace config
} // namespace qt
} // namespace scwx
