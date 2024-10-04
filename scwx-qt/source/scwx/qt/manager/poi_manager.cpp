#include <scwx/qt/manager/poi_manager.hpp>
#include <scwx/qt/types/poi_types.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/qt/main/application.hpp>
#include <scwx/util/logger.hpp>

#include <filesystem>
#include <vector>
#include <string>

#include <QStandardPaths>
#include <boost/json.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::poi_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kNameName_      = "name";
static const std::string kLatitudeName_  = "latitude";
static const std::string kLongitudeName_ = "longitude";

class POIManager::Impl
{
public:
   class POIRecord;

   explicit Impl(POIManager* self) : self_ {self} {}
   ~Impl() {}

   std::string poiSettingsPath_ {};
   std::vector<std::shared_ptr<POIRecord>> poiRecords_ {};

   POIManager* self_;

   void InitializePOISettings();
   void ReadPOISettings();
   void WritePOISettings();
   std::shared_ptr<POIRecord> GetPOIByName(const std::string& name);

};

class POIManager::Impl::POIRecord
{
public:
   POIRecord(std::string name, double latitude, double longitude) :
       name_ {name}, latitude_ {latitude}, longitude_ {longitude}
   {
   }

   std::string name_;
   double      latitude_;
   double      longitude_;

   friend void tag_invoke(boost::json::value_from_tag,
                          boost::json::value&                     jv,
                          const std::shared_ptr<POIRecord>& record)
   {
      jv = {{kNameName_, record->name_},
            {kLatitudeName_, record->latitude_},
            {kLongitudeName_, record->longitude_}};
   }

   friend POIRecord tag_invoke(boost::json::value_to_tag<POIRecord>,
                                     const boost::json::value& jv)
   {
      return POIRecord(
         boost::json::value_to<std::string>(jv.at(kNameName_)),
         boost::json::value_to<double>(jv.at(kLatitudeName_)),
         boost::json::value_to<double>(jv.at(kLongitudeName_)));
   }
};


void POIManager::Impl::InitializePOISettings()
{
   std::string appDataPath {
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         .toStdString()};

   if (!std::filesystem::exists(appDataPath))
   {
      if (!std::filesystem::create_directories(appDataPath))
      {
         logger_->error("Unable to create application data directory: \"{}\"",
                        appDataPath);
      }
   }

   poiSettingsPath_ = appDataPath + "/points_of_interest.json";
}

void POIManager::Impl::ReadPOISettings()
{
   logger_->info("Reading point of interest settings");

   boost::json::value poiJson = nullptr;

   // Determine if poi settings exists
   if (std::filesystem::exists(poiSettingsPath_))
   {
      poiJson = util::json::ReadJsonFile(poiSettingsPath_);
   }

   if (poiJson != nullptr && poiJson.is_array())
   {
      // For each poi entry
      auto& poiArray = poiJson.as_array();
      poiRecords_.reserve(poiArray.size());
      for (auto& poiEntry : poiArray)
      {
         try
         {
            POIRecord record =
               boost::json::value_to<POIRecord>(poiEntry);

            if (!record.name_.empty())
            {
               poiRecords_.emplace_back(std::make_shared<POIRecord>(
                  record.name_, record.latitude_, record.longitude_));
            }
         }
         catch (const std::exception& ex)
         {
            logger_->warn("Invalid point of interest entry: {}", ex.what());
         }
      }

      logger_->debug("{} point of interest entries", poiRecords_.size());
   }
}

void POIManager::Impl::WritePOISettings()
{
   logger_->info("Saving point of interest settings");

   auto poiJson = boost::json::value_from(poiRecords_);
   util::json::WriteJsonFile(poiSettingsPath_, poiJson);
}

std::shared_ptr<POIManager::Impl::POIRecord>
POIManager::Impl::GetPOIByName(const std::string& name)
{
   for (auto& poiRecord : poiRecords_)
   {
      if (poiRecord->name_ == name)
      {
         return poiRecord;
      }
   }

   return nullptr;
}

POIManager::POIManager() : p(std::make_unique<Impl>(this))
{
   // TODO THREADING?
   try
   {
      p->InitializePOISettings();

      // Read POI settings on startup
      //main::Application::WaitForInitialization();
      p->ReadPOISettings();
   }
   catch (const std::exception& ex)
   {
      logger_->error(ex.what());
   }
}

POIManager::~POIManager()
{
   p->WritePOISettings();
}

size_t POIManager::poi_count()
{
   return p->poiRecords_.size();
}

// TODO deal with out of range/not found
types::PointOfInterest POIManager::get_poi(size_t index)
{
   std::shared_ptr<POIManager::Impl::POIRecord> poiRecord =
      p->poiRecords_[index];
   return types::PointOfInterest(
      poiRecord->name_, poiRecord->latitude_, poiRecord->longitude_);
}

types::PointOfInterest POIManager::get_poi(const std::string& name)
{
   std::shared_ptr<POIManager::Impl::POIRecord> poiRecord =
      p->GetPOIByName(name);
   return types::PointOfInterest(
      poiRecord->name_, poiRecord->latitude_, poiRecord->longitude_);
}

void POIManager::set_poi(size_t index, const types::PointOfInterest& poi)
{
   std::shared_ptr<POIManager::Impl::POIRecord> poiRecord =
      p->poiRecords_[index];
   poiRecord->name_      = poi.name_;
   poiRecord->latitude_  = poi.latitude_;
   poiRecord->longitude_ = poi.longitude_;
}

void POIManager::set_poi(const std::string&            name,
                         const types::PointOfInterest& poi)
{
   std::shared_ptr<POIManager::Impl::POIRecord> poiRecord =
      p->GetPOIByName(name);
   poiRecord->name_      = poi.name_;
   poiRecord->latitude_  = poi.latitude_;
   poiRecord->longitude_ = poi.longitude_;
}

void POIManager::add_poi(const types::PointOfInterest& poi)
{
   p->poiRecords_.emplace_back(std::make_shared<Impl::POIRecord>(
      poi.name_, poi.latitude_, poi.longitude_));
}

void POIManager::move_poi(size_t from, size_t to)
{
   if (from >= p->poiRecords_.size() || to >= p->poiRecords_.size())
   {
      return;
   }
   std::shared_ptr<POIManager::Impl::POIRecord> poiRecord =
      p->poiRecords_[from];

   if (from == to)
   {
   }
   else if (from < to)
   {
      for (size_t i = from; i < to; i++)
      {
         p->poiRecords_[i] = p->poiRecords_[i + 1];
      }
      p->poiRecords_[to] = poiRecord;
   }
   else
   {
      for (size_t i = from; i > to; i--)
      {
         p->poiRecords_[i] = p->poiRecords_[i - 1];
      }
      p->poiRecords_[to] = poiRecord;
   }
}

std::shared_ptr<POIManager> POIManager::Instance()
{
   static std::weak_ptr<POIManager> poiManagerReference_ {};

   std::shared_ptr<POIManager> poiManager = poiManagerReference_.lock();

   if (poiManager == nullptr)
   {
      poiManager = std::make_shared<POIManager>();
      poiManagerReference_ = poiManager;
   }

   return poiManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
