#include <scwx/qt/manager/marker_manager.hpp>
#include <scwx/qt/types/marker_types.hpp>
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

static const std::string logPrefix_ = "scwx::qt::manager::marker_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kNameName_      = "name";
static const std::string kLatitudeName_  = "latitude";
static const std::string kLongitudeName_ = "longitude";

class MarkerManager::Impl
{
public:
   class MarkerRecord;

   explicit Impl(MarkerManager* self) : self_ {self} {}
   ~Impl() {}

   std::string                                markerSettingsPath_ {};
   std::vector<std::shared_ptr<MarkerRecord>> markerRecords_ {};

   MarkerManager* self_;

   void                          InitializeMarkerSettings();
   void                          ReadMarkerSettings();
   void                          WriteMarkerSettings();
   std::shared_ptr<MarkerRecord> GetMarkerByName(const std::string& name);
};

class MarkerManager::Impl::MarkerRecord
{
public:
   MarkerRecord(std::string name, double latitude, double longitude) :
       name_ {name}, latitude_ {latitude}, longitude_ {longitude}
   {
   }

   std::string name_;
   double      latitude_;
   double      longitude_;

   friend void tag_invoke(boost::json::value_from_tag,
                          boost::json::value&                  jv,
                          const std::shared_ptr<MarkerRecord>& record)
   {
      jv = {{kNameName_, record->name_},
            {kLatitudeName_, record->latitude_},
            {kLongitudeName_, record->longitude_}};
   }

   friend MarkerRecord tag_invoke(boost::json::value_to_tag<MarkerRecord>,
                                  const boost::json::value& jv)
   {
      return MarkerRecord(
         boost::json::value_to<std::string>(jv.at(kNameName_)),
         boost::json::value_to<double>(jv.at(kLatitudeName_)),
         boost::json::value_to<double>(jv.at(kLongitudeName_)));
   }
};

void MarkerManager::Impl::InitializeMarkerSettings()
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

   markerSettingsPath_ = appDataPath + "/location-markers.json";
}

void MarkerManager::Impl::ReadMarkerSettings()
{
   logger_->info("Reading location marker settings");

   boost::json::value markerJson = nullptr;

   // Determine if marker settings exists
   if (std::filesystem::exists(markerSettingsPath_))
   {
      markerJson = util::json::ReadJsonFile(markerSettingsPath_);
   }

   if (markerJson != nullptr && markerJson.is_array())
   {
      // For each marker entry
      auto& markerArray = markerJson.as_array();
      markerRecords_.reserve(markerArray.size());
      for (auto& markerEntry : markerArray)
      {
         try
         {
            MarkerRecord record =
               boost::json::value_to<MarkerRecord>(markerEntry);

            if (!record.name_.empty())
            {
               markerRecords_.emplace_back(std::make_shared<MarkerRecord>(
                  record.name_, record.latitude_, record.longitude_));
            }
         }
         catch (const std::exception& ex)
         {
            logger_->warn("Invalid location marker entry: {}", ex.what());
         }
      }

      logger_->debug("{} location marker entries", markerRecords_.size());
   }
}

void MarkerManager::Impl::WriteMarkerSettings()
{
   logger_->info("Saving location marker settings");

   auto markerJson = boost::json::value_from(markerRecords_);
   util::json::WriteJsonFile(markerSettingsPath_, markerJson);
}

std::shared_ptr<MarkerManager::Impl::MarkerRecord>
MarkerManager::Impl::GetMarkerByName(const std::string& name)
{
   for (auto& markerRecord : markerRecords_)
   {
      if (markerRecord->name_ == name)
      {
         return markerRecord;
      }
   }

   return nullptr;
}

MarkerManager::MarkerManager() : p(std::make_unique<Impl>(this))
{
   // TODO THREADING?
   try
   {
      p->InitializeMarkerSettings();

      // Read Marker settings on startup
      // main::Application::WaitForInitialization();
      p->ReadMarkerSettings();
   }
   catch (const std::exception& ex)
   {
      logger_->error(ex.what());
   }
}

MarkerManager::~MarkerManager()
{
   p->WriteMarkerSettings();
}

size_t MarkerManager::marker_count()
{
   return p->markerRecords_.size();
}

// TODO deal with out of range/not found
types::MarkerInfo MarkerManager::get_marker(size_t index)
{
   std::shared_ptr<MarkerManager::Impl::MarkerRecord> markerRecord =
      p->markerRecords_[index];
   return types::MarkerInfo(
      markerRecord->name_, markerRecord->latitude_, markerRecord->longitude_);
}

types::MarkerInfo MarkerManager::get_marker(const std::string& name)
{
   std::shared_ptr<MarkerManager::Impl::MarkerRecord> markerRecord =
      p->GetMarkerByName(name);
   return types::MarkerInfo(
      markerRecord->name_, markerRecord->latitude_, markerRecord->longitude_);
}

void MarkerManager::set_marker(size_t index, const types::MarkerInfo& marker)
{
   std::shared_ptr<MarkerManager::Impl::MarkerRecord> markerRecord =
      p->markerRecords_[index];
   markerRecord->name_      = marker.name_;
   markerRecord->latitude_  = marker.latitude_;
   markerRecord->longitude_ = marker.longitude_;
}

void MarkerManager::set_marker(const std::string&       name,
                               const types::MarkerInfo& marker)
{
   std::shared_ptr<MarkerManager::Impl::MarkerRecord> markerRecord =
      p->GetMarkerByName(name);
   markerRecord->name_      = marker.name_;
   markerRecord->latitude_  = marker.latitude_;
   markerRecord->longitude_ = marker.longitude_;
}

void MarkerManager::add_marker(const types::MarkerInfo& marker)
{
   p->markerRecords_.emplace_back(std::make_shared<Impl::MarkerRecord>(
      marker.name_, marker.latitude_, marker.longitude_));
}

void MarkerManager::move_marker(size_t from, size_t to)
{
   if (from >= p->markerRecords_.size() || to >= p->markerRecords_.size())
   {
      return;
   }
   std::shared_ptr<MarkerManager::Impl::MarkerRecord> markerRecord =
      p->markerRecords_[from];

   if (from == to) {}
   else if (from < to)
   {
      for (size_t i = from; i < to; i++)
      {
         p->markerRecords_[i] = p->markerRecords_[i + 1];
      }
      p->markerRecords_[to] = markerRecord;
   }
   else
   {
      for (size_t i = from; i > to; i--)
      {
         p->markerRecords_[i] = p->markerRecords_[i - 1];
      }
      p->markerRecords_[to] = markerRecord;
   }
}

std::shared_ptr<MarkerManager> MarkerManager::Instance()
{
   static std::weak_ptr<MarkerManager> markerManagerReference_ {};

   std::shared_ptr<MarkerManager> markerManager =
      markerManagerReference_.lock();

   if (markerManager == nullptr)
   {
      markerManager           = std::make_shared<MarkerManager>();
      markerManagerReference_ = markerManager;
   }

   return markerManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
