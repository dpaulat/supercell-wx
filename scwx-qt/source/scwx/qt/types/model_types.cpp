#include <scwx/qt/types/model_types.hpp>

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QTimeZone>

namespace scwx::qt::types
{
namespace
{

QVector<int> ParseIntegerArray(const QJsonValue& value)
{
   QVector<int> output;
   for (const auto& item : value.toArray())
   {
      output.push_back(item.toInt());
   }
   return output;
}

ModelProduct ParseProduct(const QJsonObject& object)
{
   return {.id_           = object.value("id").toString(),
           .title_        = object.value("title").toString(),
           .kind_         = object.value("kind").toString(),
           .heavy_        = object.value("heavy").toBool(false),
           .experimental_ = object.value("experimental").toBool(false),
           .mapOverlaySupported_ =
              object.value("map_overlay_supported").toBool(true)};
}

std::chrono::system_clock::time_point ParseValidTime(const QString& run,
                                                     int            hour)
{
   const auto parts = run.split('_');
   if (parts.size() != 2 || !parts[1].endsWith('z'))
   {
      return {};
   }
   const auto date  = QDate::fromString(parts[0], "yyyyMMdd");
   bool       ok    = false;
   const int  cycle = parts[1].first(parts[1].size() - 1).toInt(&ok);
   if (!date.isValid() || !ok)
   {
      return {};
   }
   const auto valid = QDateTime(date, QTime(cycle, 0), QTimeZone::UTC)
                         .addSecs(static_cast<qint64>(hour) * 3600);
   return std::chrono::system_clock::from_time_t(valid.toSecsSinceEpoch());
}

} // namespace

QVector<ForecastModel> ParseModelCapabilities(const QJsonObject& data,
                                              QString&           error)
{
   QVector<ForecastModel> models;
   if (!data.value("models").isArray())
   {
      error = "Capabilities response has no models array";
      return models;
   }

   for (const auto& value : data.value("models").toArray())
   {
      const auto    object = value.toObject();
      ForecastModel model {
         .id_              = object.value("id").toString(),
         .description_     = object.value("description").toString(),
         .cycleHoursUtc_   = ParseIntegerArray(object.value("cycle_hours_utc")),
         .maxForecastHour_ = object.value("max_forecast_hour").toInt(),
         .ingestSupported_ = object.value("ingest_supported").toBool(),
      };
      for (const auto& source : object.value("sources").toArray())
      {
         model.sources_.push_back(source.toString());
      }
      for (const auto& product : object.value("products").toArray())
      {
         model.products_.push_back(ParseProduct(product.toObject()));
      }
      if (!model.id_.isEmpty())
      {
         models.push_back(std::move(model));
      }
   }
   if (models.isEmpty())
   {
      error = "Capabilities response contains no models";
   }
   return models;
}

ModelProbeResult ParseModelProbe(const QJsonObject& data, QString& error)
{
   ModelProbeResult result {
      .model_         = data.value("model").toString(),
      .date_          = data.value("date").toString(),
      .cycleUtc_      = data.value("cycle_utc").toInt(-1),
      .source_        = data.value("source").toString(),
      .forecastHours_ = ParseIntegerArray(data.value("forecast_hours")),
      .supportedForecastHours_ =
         ParseIntegerArray(data.value("supported_forecast_hours")),
      .forecastHoursComplete_ =
         data.value("forecast_hours_complete").toBool(true),
   };
   if (result.model_.isEmpty() || result.date_.isEmpty() ||
       result.cycleUtc_ < 0)
   {
      error = "Probe response is incomplete";
   }
   return result;
}

ModelRuns ParseModelRuns(const QJsonObject& data, QString& error)
{
   ModelRuns result {.model_ = data.value("model").toString()};
   for (const auto& value : data.value("runs").toArray())
   {
      const auto object = value.toObject();
      ModelRun   run {.run_ = object.value("run").toString(),
                      .forecastHours_ =
                       ParseIntegerArray(object.value("forecast_hours"))};
      if (!run.run_.isEmpty() && !run.forecastHours_.isEmpty())
      {
         result.runs_.push_back(std::move(run));
      }
   }
   if (result.model_.isEmpty())
   {
      error = "Stored-runs response is incomplete";
   }
   return result;
}

ModelCatalog ParseModelCatalog(const QJsonObject& data, QString& error)
{
   ModelCatalog result {.model_ = data.value("model").toString(),
                        .run_   = data.value("run").toString(),
                        .hour_  = data.value("hour").toInt(-1),
                        .storedHours_ =
                           ParseIntegerArray(data.value("stored_hours"))};
   for (const auto& value : data.value("products").toArray())
   {
      auto object = value.toObject();
      result.products_.push_back({.id_    = object.value("id").toString(),
                                  .title_ = object.value("title").toString(
                                     object.value("id").toString()),
                                  .kind_ = object.value("kind").toString()});
   }
   if (result.model_.isEmpty() || result.run_.isEmpty() || result.hour_ < 0)
   {
      error = "Catalog response is incomplete";
   }
   return result;
}

ModelFrame ParseModelFrame(const QJsonObject& data,
                           const QString&     run,
                           const QString&     model,
                           QString&           error)
{
   const auto image  = data.value("image").toObject();
   const auto bounds = image.value("bounds_wsen").toArray();
   ModelFrame frame {.model_        = model,
                     .run_          = run,
                     .product_      = data.value("product").toString(),
                     .path_         = data.value("path").toString(),
                     .forecastHour_ = data.value("hour").toInt(-1),
                     .width_        = image.value("width").toInt(),
                     .height_       = image.value("height").toInt()};
   if (bounds.size() == 4)
   {
      frame.west_  = bounds[0].toDouble();
      frame.south_ = bounds[1].toDouble();
      frame.east_  = bounds[2].toDouble();
      frame.north_ = bounds[3].toDouble();
   }
   frame.validTime_ = ParseValidTime(run, frame.forecastHour_);
   if (frame.product_.isEmpty() || frame.path_.isEmpty() ||
       frame.forecastHour_ < 0 || bounds.size() != 4)
   {
      error = "Rendered-frame response is incomplete";
   }
   return frame;
}

ModelSounding ParseModelSounding(const QJsonObject& data, QString& error)
{
   const auto    image    = data.value("image").toObject();
   const auto    metadata = data.value("metadata").toObject();
   ModelSounding sounding {
      .model_              = data.value("model").toString(),
      .run_                = data.value("run").toString(),
      .path_               = data.value("path").toString(),
      .station_            = metadata.value("station").toString(),
      .validTime_          = metadata.value("valid_time").toString(),
      .forecastHour_       = data.value("hour").toInt(-1),
      .width_              = image.value("width").toInt(),
      .height_             = image.value("height").toInt(),
      .requestedLatitude_  = data.value("requested_latitude").toDouble(),
      .requestedLongitude_ = data.value("requested_longitude").toDouble(),
      .selectedLatitude_   = data.value("selected_latitude").toDouble(),
      .selectedLongitude_  = data.value("selected_longitude").toDouble(),
   };
   if (sounding.model_.isEmpty() || sounding.run_.isEmpty() ||
       sounding.path_.isEmpty() || sounding.forecastHour_ < 0 ||
       !data.value("requested_latitude").isDouble() ||
       !data.value("requested_longitude").isDouble() ||
       !data.value("selected_latitude").isDouble() ||
       !data.value("selected_longitude").isDouble())
   {
      error = "Sounding response is incomplete";
   }
   return sounding;
}

} // namespace scwx::qt::types
