#include <scwx/qt/manager/poi_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/qt/util/json.hpp>

#include <filesystem>

#include <boost/json.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::placefile_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kNameName_      = "name";
static const std::string kLatitudeName_  = "latitude";
static const std::string kLongitudeName_ = "longitude";

class POIManager::Impl
{
public:
   class PointOfInterest;

   explicit Impl(POIManager* self) : self_ {self} {}

   std::string poiSettingsPath_ {};

   POIManager* self_;

   void ReadPOISettings();

};

class POIManager::Impl::PointOfInterest
{
public:
   PointOfInterest(std::string name, double latitude, double longitude) :
       name_ {name}, latitude_ {latitude}, longitude_ {longitude}
   {
   }

   std::string name_;
   double      latitude_;
   double      longitude_;

   friend void tag_invoke(boost::json::value_from_tag,
                          boost::json::value&                     jv,
                          const std::shared_ptr<PointOfInterest>& record)
   {
      jv = {{kNameName_, record->name_},
            {kLatitudeName_, record->latitude_},
            {kLongitudeName_, record->longitude_}};
   }

   friend PointOfInterest tag_invoke(boost::json::value_to_tag<PointOfInterest>,
                                     const boost::json::value& jv)
   {
      return PointOfInterest(
         boost::json::value_to<std::string>(jv.at(kNameName_)),
         boost::json::value_to<double>(jv.at(kLatitudeName_)),
         boost::json::value_to<double>(jv.at(kLongitudeName_)));
   }
};

POIManager::POIManager() : p(std::make_unique<Impl>(this)) {}

void POIManager::Impl::ReadPOISettings()
{
   logger_->info("Reading point of intrest settings");

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
      for (auto& poiEntry : poiArray)
      {
         try
         {
            PointOfInterest record =
               boost::json::value_to<PointOfInterest>(poiEntry);

            if (!record.name_.empty())
            {
               // Add record
            }
         }
         catch (const std::exception& ex)
         {
            logger_->warn("Invalid point of interest entry: {}", ex.what());
         }
      }
   }
}

} // namespace manager
} // namespace qt
} // namespace scwx
