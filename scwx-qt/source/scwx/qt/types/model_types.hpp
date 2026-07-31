#pragma once

#include <chrono>

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace scwx::qt::types
{

inline constexpr auto kModelBridgeSchema = "rusty-weather.model-bridge.v1";

struct ModelProduct
{
   QString id_;
   QString title_;
   QString kind_;
   bool    heavy_ {false};
   bool    experimental_ {false};
   bool    mapOverlaySupported_ {true};
};

struct ForecastModel
{
   QString               id_;
   QString               description_;
   QVector<int>          cycleHoursUtc_;
   int                   maxForecastHour_ {0};
   bool                  ingestSupported_ {false};
   QStringList           sources_;
   QVector<ModelProduct> products_;
};

struct ModelProbeResult
{
   QString      model_;
   QString      date_;
   int          cycleUtc_ {0};
   QString      source_;
   QVector<int> forecastHours_;
   QVector<int> supportedForecastHours_;
   bool         forecastHoursComplete_ {true};
};

struct ModelRun
{
   QString      run_;
   QVector<int> forecastHours_;
};

struct ModelRuns
{
   QString           model_;
   QVector<ModelRun> runs_;
};

struct ModelCatalog
{
   QString               model_;
   QString               run_;
   int                   hour_ {0};
   QVector<int>          storedHours_;
   QVector<ModelProduct> products_;
};

struct ModelFrame
{
   QString                               model_;
   QString                               run_;
   QString                               product_;
   QString                               path_;
   int                                   forecastHour_ {0};
   int                                   width_ {0};
   int                                   height_ {0};
   double                                west_ {0.0};
   double                                east_ {0.0};
   double                                south_ {0.0};
   double                                north_ {0.0};
   std::chrono::system_clock::time_point validTime_ {};
};

struct ModelSounding
{
   QString model_;
   QString run_;
   QString path_;
   QString station_;
   QString validTime_;
   int     forecastHour_ {0};
   int     width_ {0};
   int     height_ {0};
   double  requestedLatitude_ {0.0};
   double  requestedLongitude_ {0.0};
   double  selectedLatitude_ {0.0};
   double  selectedLongitude_ {0.0};
};

[[nodiscard]] QVector<ForecastModel>
ParseModelCapabilities(const QJsonObject& data, QString& error);
[[nodiscard]] ModelProbeResult ParseModelProbe(const QJsonObject& data,
                                               QString&           error);
[[nodiscard]] ModelRuns ParseModelRuns(const QJsonObject& data, QString& error);
[[nodiscard]] ModelCatalog  ParseModelCatalog(const QJsonObject& data,
                                              QString&           error);
[[nodiscard]] ModelFrame    ParseModelFrame(const QJsonObject& data,
                                            const QString&     run,
                                            const QString&     model,
                                            QString&           error);
[[nodiscard]] ModelSounding ParseModelSounding(const QJsonObject& data,
                                               QString&           error);

} // namespace scwx::qt::types
