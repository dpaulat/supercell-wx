#include <scwx/qt/types/layer_types.hpp>
#include <scwx/qt/types/model_types.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

#include <gtest/gtest.h>

namespace scwx::qt::types
{

TEST(ModelTypesTest, ParsesCapabilitiesContract)
{
   const auto document = QJsonDocument::fromJson(R"json(
      {"models":[{"id":"hrrr","description":"HRRR",
       "cycle_hours_utc":[0,1,2,3],"max_forecast_hour":48,
       "ingest_supported":true,"sources":["aws"],
       "products":[{"id":"2m_temperature","title":"2 m Temperature",
       "kind":"direct","heavy":false,"experimental":false,
       "map_overlay_supported":true},
       {"id":"precipitation_type","title":"Precipitation Type",
       "kind":"direct","map_overlay_supported":false}]}]}
   )json");
   QString    error;
   const auto models = ParseModelCapabilities(document.object(), error);

   ASSERT_TRUE(error.isEmpty()) << error.toStdString();
   ASSERT_EQ(models.size(), 1);
   EXPECT_EQ(models[0].id_, "hrrr");
   EXPECT_TRUE(models[0].ingestSupported_);
   ASSERT_EQ(models[0].products_.size(), 2);
   EXPECT_TRUE(models[0].products_[0].mapOverlaySupported_);
   EXPECT_FALSE(models[0].products_[1].mapOverlaySupported_);
}

TEST(ModelTypesTest, ParsesGeoreferencedFrameAndValidTime)
{
   const auto document = QJsonDocument::fromJson(R"json(
      {"hour":6,"product":"2m_temperature","path":"overlay.png",
       "image":{"width":1600,"height":900,
       "bounds_wsen":[-100.0,30.0,-90.0,40.0]}}
   )json");
   QString    error;
   const auto frame =
      ParseModelFrame(document.object(), "20260730_12z", "hrrr", error);

   ASSERT_TRUE(error.isEmpty()) << error.toStdString();
   EXPECT_EQ(frame.forecastHour_, 6);
   EXPECT_DOUBLE_EQ(frame.west_, -100.0);
   EXPECT_DOUBLE_EQ(frame.north_, 40.0);
   const auto expected = std::chrono::system_clock::from_time_t(
      QDateTime(QDate(2026, 7, 30), QTime(18, 0), QTimeZone::UTC)
         .toSecsSinceEpoch());
   EXPECT_EQ(frame.validTime_, expected);
}

TEST(ModelTypesTest, ParsesStoredRunsForOfflineUse)
{
   const auto document = QJsonDocument::fromJson(R"json(
      {"model":"hrrr","runs":[
       {"run":"20260730_18z","forecast_hours":[0,1,2,3]},
       {"run":"20260730_12z","forecast_hours":[0,3,6]}]}
   )json");
   QString    error;
   const auto runs = ParseModelRuns(document.object(), error);

   ASSERT_TRUE(error.isEmpty()) << error.toStdString();
   ASSERT_EQ(runs.runs_.size(), 2);
   EXPECT_EQ(runs.model_, "hrrr");
   EXPECT_EQ(runs.runs_[0].run_, "20260730_18z");
   EXPECT_EQ(runs.runs_[1].forecastHours_.back(), 6);
}

TEST(ModelTypesTest, ParsesFastProbeContract)
{
   const auto document = QJsonDocument::fromJson(R"json(
      {"model":"hrrr","date":"20260731","cycle_utc":6,
       "source":"nomads","forecast_hours":[3],
       "forecast_hours_complete":false,
       "supported_forecast_hours":[0,1,2,3,4,5,6]}
   )json");
   QString    error;
   const auto probe = ParseModelProbe(document.object(), error);

   ASSERT_TRUE(error.isEmpty()) << error.toStdString();
   ASSERT_EQ(probe.forecastHours_.size(), 1);
   EXPECT_EQ(probe.forecastHours_.front(), 3);
   EXPECT_FALSE(probe.forecastHoursComplete_);
   ASSERT_EQ(probe.supportedForecastHours_.size(), 7);
   EXPECT_EQ(probe.supportedForecastHours_.back(), 6);
}

TEST(ModelTypesTest, LegacyProbeContractRemainsComplete)
{
   const auto document = QJsonDocument::fromJson(R"json(
      {"model":"gfs","date":"20260731","cycle_utc":0,
       "source":"nomads","forecast_hours":[0,1,2,3]}
   )json");
   QString    error;
   const auto probe = ParseModelProbe(document.object(), error);

   ASSERT_TRUE(error.isEmpty()) << error.toStdString();
   EXPECT_TRUE(probe.forecastHoursComplete_);
   EXPECT_TRUE(probe.supportedForecastHours_.isEmpty());
   ASSERT_EQ(probe.forecastHours_.size(), 4);
   EXPECT_EQ(probe.forecastHours_.back(), 3);
}

TEST(ModelTypesTest, ParsesRenderedSounding)
{
   const auto document = QJsonDocument::fromJson(R"json(
      {"model":"hrrr","run":"20260730_18z","hour":3,
       "path":"sounding.png",
       "requested_latitude":35.333,"requested_longitude":-97.278,
       "selected_latitude":35.321,"selected_longitude":-97.291,
       "image":{"width":2048,"height":1152},
       "metadata":{"station":"grid point","valid_time":"2026-07-30T21:00:00Z"}}
   )json");
   QString    error;
   const auto sounding = ParseModelSounding(document.object(), error);

   ASSERT_TRUE(error.isEmpty()) << error.toStdString();
   EXPECT_EQ(sounding.model_, "hrrr");
   EXPECT_EQ(sounding.run_, "20260730_18z");
   EXPECT_EQ(sounding.forecastHour_, 3);
   EXPECT_DOUBLE_EQ(sounding.requestedLongitude_, -97.278);
   EXPECT_DOUBLE_EQ(sounding.selectedLatitude_, 35.321);
   EXPECT_EQ(sounding.width_, 2048);
   EXPECT_EQ(sounding.station_, "grid point");
   EXPECT_EQ(sounding.validTime_, "2026-07-30T21:00:00Z");
}

TEST(ModelTypesTest, SoundingMetadataIsOptional)
{
   const auto document = QJsonDocument::fromJson(R"json(
      {"model":"gfs","run":"20260730_12z","hour":6,
       "path":"sounding.png",
       "requested_latitude":40.0,"requested_longitude":-105.0,
       "selected_latitude":40.01,"selected_longitude":-104.99,
       "image":{"width":1600,"height":900}}
   )json");
   QString    error;
   const auto sounding = ParseModelSounding(document.object(), error);

   EXPECT_TRUE(error.isEmpty()) << error.toStdString();
   EXPECT_TRUE(sounding.station_.isEmpty());
   EXPECT_TRUE(sounding.validTime_.isEmpty());
}

TEST(ModelTypesTest, ForecastModelLayerHasStableName)
{
   EXPECT_EQ(GetDataLayerName(DataLayer::ModelField), "Forecast Model");
   EXPECT_EQ(GetDataLayer("forecast model"), DataLayer::ModelField);
}

} // namespace scwx::qt::types
