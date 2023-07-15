#include <scwx/qt/map/map_provider.hpp>
#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#include <regex>

#include <QCoreApplication>
#include <QMapLibreGL/QMapLibreGL>
#include <QTimer>

#include <gtest/gtest.h>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ {"scwx::qt::map::map_provider.test"};
static const auto        logger_ = scwx::util::Logger::Create(logPrefix_);

class ByMapProviderTest :
    public testing::TestWithParam<std::pair<MapProvider, std::string>>
{
};

TEST_P(ByMapProviderTest, MapProviderLayers)
{
   auto& [mapProvider, apiKeyName] = GetParam();

   // Configure API key
   std::string apiKey = scwx::util::GetEnvironment(apiKeyName);
   if (apiKey.empty())
   {
      logger_->info("API key not set, skipping test");
      EXPECT_EQ(true, true);
      return;
   }

   // Setup QCoreApplication
   int              argc   = 1;
   const char*      argv[] = {"arg", nullptr};
   QCoreApplication application(argc, const_cast<char**>(argv));

   // Configure map provider
   const MapProviderInfo& mapProviderInfo = GetMapProviderInfo(mapProvider);

   // Configure QMapLibreGL
   QMapLibreGL::Settings mapSettings {};
   mapSettings.resetToTemplate(mapProviderInfo.settingsTemplate_);
   mapSettings.setApiKey(QString::fromStdString(apiKey));

   QMapLibreGL::Map map(nullptr, mapSettings, QSize(1, 1));
   application.processEvents();

   // Connect style load completion signal
   QObject::connect(
      &map,
      &QMapLibreGL::Map::mapChanged,
      [&](QMapLibreGL::Map::MapChange mapChange)
      {
         if (mapChange ==
             QMapLibreGL::Map::MapChange::MapChangeDidFinishLoadingStyle)
         {
            application.exit();
         }
      });

   // Connect timeout timer
   bool   timeout = false;
   QTimer timeoutTimer {};
   timeoutTimer.setSingleShot(true);
   QObject::connect(&timeoutTimer,
                    &QTimer::timeout,
                    [&]()
                    {
                       // Reached timeout
                       logger_->warn("Timed out waiting for style change");
                       timeout = true;

                       application.exit();
                    });

   // Iterate through each style
   for (const auto& mapStyle : mapProviderInfo.mapStyles_)
   {
      using namespace std::chrono_literals;

      // Load style
      timeout = false;
      map.setStyleUrl(QString::fromStdString(mapStyle.url_));
      timeoutTimer.start(5000ms);
      application.exec();
      timeoutTimer.stop();

      // Check result
      if (!timeout)
      {
         // Print layer names for debug
         std::string layerIdsString = map.layerIds().join(", ").toStdString();
         logger_->debug("{} Layers: [{}]", mapStyle.name_, layerIdsString);

         // Search layer list
         bool foundMatch = false;
         for (const QString& qlayer : map.layerIds())
         {
            const std::string layer = qlayer.toStdString();

            // Draw below layers defined in map style
            auto it = std::find_if(
               mapStyle.drawBelow_.cbegin(),
               mapStyle.drawBelow_.cend(),
               [&layer](const std::string& styleLayer) -> bool
               {
                  std::regex re {styleLayer, std::regex_constants::icase};
                  return std::regex_match(layer, re);
               });

            if (it != mapStyle.drawBelow_.cend())
            {
               foundMatch = true;
               break;
            }
         }

         // Check match
         EXPECT_EQ(foundMatch, true);

         if (!foundMatch)
         {
            logger_->error("Could not find drawBelow in style {}",
                           mapStyle.name_);
         }
      }
      else
      {
         EXPECT_EQ(timeout, false);
      }
   }

   EXPECT_EQ(false, false);
}

INSTANTIATE_TEST_SUITE_P(
   MapProviderTest,
   ByMapProviderTest,
   testing::Values(std::make_pair(MapProvider::Mapbox, "MAPBOX_API_KEY"),
                   std::make_pair(MapProvider::MapTiler, "MAPTILER_API_KEY")));

} // namespace map
} // namespace qt
} // namespace scwx
