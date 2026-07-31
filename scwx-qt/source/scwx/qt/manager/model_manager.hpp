#pragma once

#include <scwx/qt/types/model_types.hpp>

#include <chrono>
#include <memory>

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace scwx::qt::manager
{

class ModelManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(ModelManager)

public:
   explicit ModelManager();
   ~ModelManager();

   static std::shared_ptr<ModelManager> Instance();

   [[nodiscard]] QString                       bridge_path() const;
   [[nodiscard]] QString                       store_root() const;
   [[nodiscard]] QString                       cache_root() const;
   [[nodiscard]] bool                          busy() const;
   [[nodiscard]] QVector<types::ForecastModel> models() const;
   [[nodiscard]] QSet<QString>                 favorites() const;
   [[nodiscard]] float                         opacity() const;
   [[nodiscard]] bool                          visible() const;
   [[nodiscard]] bool                          timeline_sync() const;

public slots:
   void SetBridgePath(const QString& path);
   void SetFavorite(const QString& product, bool favorite);
   void SetOpacity(float opacity);
   void SetVisible(bool visible);
   void SetTimelineSync(bool enabled);
   void SelectForecastHour(int hour);

   void LoadCapabilities();
   void Probe(const QString& model,
              const QDate&   date,
              int            forecastHour,
              const QString& source = {});
   void Fetch(const QString& model,
              const QString& date,
              int            cycleUtc,
              const QString& hours,
              const QString& source,
              const QString& profile,
              bool           heavy,
              bool           verify);
   void ListRuns(const QString& model);
   void Catalog(const QString& model, const QString& run, int forecastHour);
   void Render(const QString&     model,
               const QString&     run,
               const QString&     hours,
               const QStringList& products,
               double             west,
               double             east,
               double             south,
               double             north,
               int                width,
               const QString&     source = {});
   void Sounding(const QString& model,
                 const QString& run,
                 int            forecastHour,
                 double         latitude,
                 double         longitude);
   void CacheStatus();
   void Cancel();

signals:
   void CapabilitiesUpdated(QVector<types::ForecastModel> models);
   void ProbeCompleted(types::ModelProbeResult result);
   void FetchHourCompleted(int hour, QString path);
   void FetchCompleted(QString      model,
                       QString      run,
                       QString      source,
                       QVector<int> hours);
   void RunsUpdated(types::ModelRuns runs);
   void CatalogUpdated(types::ModelCatalog catalog);
   void FrameAvailable(types::ModelFrame frame);
   void SoundingAvailable(types::ModelSounding sounding);
   void FrameSelected(types::ModelFrame frame);
   void FramesSelected(QVector<types::ModelFrame> frames);
   void CacheStatusUpdated(quint64 bytes, quint64 files);
   void ProgressUpdated(QString operation,
                        int     completed,
                        int     total,
                        QString message);
   void StatusUpdated(QString status, QDateTime updatedAt);
   void OperationStateChanged(bool busy, QString operation);
   void ErrorOccurred(QString message);
   void OpacityChanged(float opacity);
   void VisibilityChanged(bool visible);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
