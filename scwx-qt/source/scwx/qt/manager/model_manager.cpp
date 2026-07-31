#include <scwx/qt/manager/model_manager.hpp>

#include <scwx/qt/main/application_paths.hpp>
#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace scwx::qt::manager
{
namespace
{

static const std::string logPrefix_ = "scwx::qt::manager::model_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

QString ResolveDefaultBridgePath()
{
#ifdef Q_OS_WIN
   const QString executableName = "rw_model_bridge.exe";
#else
   const QString executableName = "rw_model_bridge";
#endif
   const QString adjacent =
      QDir(QCoreApplication::applicationDirPath()).filePath(executableName);
   if (QFileInfo::exists(adjacent))
   {
      return adjacent;
   }
   const QString onPath = QStandardPaths::findExecutable(executableName);
   return onPath.isEmpty() ? adjacent : onPath;
}

} // namespace

class ModelManager::Impl
{
public:
   enum class Operation
   {
      None,
      Capabilities,
      Probe,
      Fetch,
      Runs,
      Catalog,
      Render,
      Sounding,
      CacheStatus
   };

   explicit Impl(ModelManager* self) : self_ {self}
   {
      const auto settingsDirectory = main::ApplicationPaths::GetLocation(
         main::ApplicationPaths::StandardLocation::Settings);
      std::filesystem::create_directories(settingsDirectory);
      settings_ = std::make_unique<QSettings>(
         QString::fromStdString(
            (settingsDirectory / "model-processing.ini").string()),
         QSettings::IniFormat);

      bridgePath_ =
         settings_->value("bridge/path", ResolveDefaultBridgePath()).toString();
      const auto favoriteList =
         settings_->value("products/favorites").toStringList();
      favorites_    = QSet<QString>(favoriteList.cbegin(), favoriteList.cend());
      opacity_      = settings_->value("display/opacity", 0.8).toFloat();
      visible_      = settings_->value("display/visible", true).toBool();
      timelineSync_ = settings_->value("display/timelineSync", true).toBool();

      const auto local = main::ApplicationPaths::GetLocation(
         main::ApplicationPaths::StandardLocation::Local);
      const auto cache = main::ApplicationPaths::GetLocation(
         main::ApplicationPaths::StandardLocation::Cache);
      storeRoot_ =
         QString::fromStdString((local / "models" / "store").string());
      cacheRoot_ = QString::fromStdString((cache / "models" / "grib").string());
      outputRoot_ =
         QString::fromStdString((cache / "models" / "overlays").string());

      process_.setProcessChannelMode(QProcess::SeparateChannels);
      QObject::connect(&process_,
                       &QProcess::readyReadStandardOutput,
                       self_,
                       [this]() { ReadStandardOutput(); });
      QObject::connect(&process_,
                       &QProcess::readyReadStandardError,
                       self_,
                       [this]()
                       { stderrBuffer_ += process_.readAllStandardError(); });
      QObject::connect(
         &process_,
         qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
         self_,
         [this](int exitCode, QProcess::ExitStatus status)
         { ProcessFinished(exitCode, status); });
      QObject::connect(&process_,
                       &QProcess::errorOccurred,
                       self_,
                       [this](QProcess::ProcessError processError)
                       {
                          if (!cancelled_)
                          {
                             const QString message = process_.errorString();
                             Q_EMIT self_->ErrorOccurred(message);
                             if (processError == QProcess::FailedToStart)
                             {
                                Q_EMIT self_->StatusUpdated(
                                   message, QDateTime::currentDateTime());
                                operation_ = Operation::None;
                                Q_EMIT self_->OperationStateChanged(false, {});
                             }
                          }
                       });

      auto timeline = TimelineManager::Instance();
      QObject::connect(timeline.get(),
                       &TimelineManager::SelectedTimeUpdated,
                       self_,
                       [this](std::chrono::system_clock::time_point time)
                       {
                          if (timelineSync_)
                          {
                             SelectNearestTime(time);
                          }
                       });
   }

   void SaveSettings()
   {
      settings_->setValue("bridge/path", bridgePath_);
      settings_->setValue("products/favorites",
                          QStringList(favorites_.values()));
      settings_->setValue("display/opacity", opacity_);
      settings_->setValue("display/visible", visible_);
      settings_->setValue("display/timelineSync", timelineSync_);
      settings_->sync();
   }

   QString OperationName() const
   {
      switch (operation_)
      {
      case Operation::Capabilities:
         return "capabilities";
      case Operation::Probe:
         return "probe";
      case Operation::Fetch:
         return "fetch";
      case Operation::Runs:
         return "stored runs";
      case Operation::Catalog:
         return "catalog";
      case Operation::Render:
         return "render";
      case Operation::Sounding:
         return "sounding";
      case Operation::CacheStatus:
         return "cache status";
      default:
         return {};
      }
   }

   bool Start(Operation operation, const QStringList& arguments)
   {
      if (process_.state() != QProcess::NotRunning)
      {
         Q_EMIT self_->ErrorOccurred(
            "Another model operation is already running");
         return false;
      }
      if (!QFileInfo::exists(bridgePath_))
      {
         Q_EMIT self_->ErrorOccurred(
            QString("Rusty Weather model bridge was not found at %1")
               .arg(bridgePath_));
         return false;
      }
      operation_ = operation;
      cancelled_ = false;
      stdoutBuffer_.clear();
      stderrBuffer_.clear();
      Q_EMIT self_->OperationStateChanged(true, OperationName());
      process_.start(bridgePath_, arguments);
      return true;
   }

   void ReadStandardOutput()
   {
      stdoutBuffer_ += process_.readAllStandardOutput();
      qsizetype newline = -1;
      while ((newline = stdoutBuffer_.indexOf('\n')) >= 0)
      {
         const QByteArray line = stdoutBuffer_.first(newline).trimmed();
         stdoutBuffer_.remove(0, newline + 1);
         if (!line.isEmpty())
         {
            HandleLine(line);
         }
      }
   }

   void HandleLine(const QByteArray& line)
   {
      QJsonParseError parseError;
      const auto      document = QJsonDocument::fromJson(line, &parseError);
      if (parseError.error != QJsonParseError::NoError || !document.isObject())
      {
         logger_->warn("Ignoring invalid model bridge output: {}",
                       line.toStdString());
         return;
      }
      const auto root = document.object();
      if (root.value("schema").toString() != types::kModelBridgeSchema)
      {
         Q_EMIT self_->ErrorOccurred("Unsupported model bridge schema");
         return;
      }
      const auto event = root.value("event").toString();
      const auto data  = root.value("data").toObject();
      QString    error;

      if (event == "capabilities")
      {
         models_ = types::ParseModelCapabilities(data, error);
         if (error.isEmpty())
            Q_EMIT self_->CapabilitiesUpdated(models_);
      }
      else if (event == "probe_complete")
      {
         auto result = types::ParseModelProbe(data, error);
         if (error.isEmpty())
            Q_EMIT self_->ProbeCompleted(result);
      }
      else if (event == "fetch_progress")
      {
         Q_EMIT self_->ProgressUpdated(
            "fetch",
            0,
            0,
            data.value("message").toString(
               QString("F%1: %2 %3")
                  .arg(data.value("hour").toInt(), 3, 10, QChar('0'))
                  .arg(data.value("stage").toString(),
                       data.value("state").toString())));
      }
      else if (event == "fetch_hour_complete")
      {
         Q_EMIT self_->FetchHourCompleted(data.value("hour").toInt(),
                                          data.value("store_path").toString());
      }
      else if (event == "fetch_complete")
      {
         QVector<int> hours;
         for (const auto& hour : data.value("hours").toArray())
         {
            hours.push_back(hour.toObject().value("hour").toInt());
         }
         currentModel_ = data.value("model").toString();
         currentRun_   = data.value("run").toString();
         Q_EMIT self_->FetchCompleted(
            currentModel_, currentRun_, data.value("source").toString(), hours);
      }
      else if (event == "runs_complete")
      {
         auto runs = types::ParseModelRuns(data, error);
         if (error.isEmpty())
            Q_EMIT self_->RunsUpdated(runs);
      }
      else if (event == "catalog_complete")
      {
         auto catalog = types::ParseModelCatalog(data, error);
         if (error.isEmpty())
            Q_EMIT self_->CatalogUpdated(catalog);
      }
      else if (event == "render_started")
      {
         Q_EMIT self_->ProgressUpdated("render",
                                       0,
                                       data.value("planned_items").toInt(),
                                       "Rendering model overlays");
      }
      else if (event == "render_item_started")
      {
         Q_EMIT self_->ProgressUpdated("render",
                                       data.value("completed").toInt(),
                                       data.value("total").toInt(),
                                       data.value("product").toString());
      }
      else if (event == "render_item_complete")
      {
         auto frame =
            types::ParseModelFrame(data, currentRun_, currentModel_, error);
         if (error.isEmpty())
         {
            auto duplicate = std::find_if(
               frames_.begin(),
               frames_.end(),
               [&frame](const auto& existing)
               {
                  return existing.run_ == frame.run_ &&
                         existing.product_ == frame.product_ &&
                         existing.forecastHour_ == frame.forecastHour_;
               });
            if (duplicate == frames_.end())
            {
               frames_.push_back(frame);
            }
            else
            {
               *duplicate = frame;
            }
            Q_EMIT self_->FrameAvailable(frame);
            Q_EMIT self_->ProgressUpdated("render",
                                          data.value("completed").toInt(),
                                          data.value("total").toInt(),
                                          frame.product_);
         }
      }
      else if (event == "render_item_skipped")
      {
         Q_EMIT self_->ProgressUpdated(
            "render",
            data.value("completed").toInt(),
            data.value("total").toInt(),
            QString("%1: %2").arg(data.value("product").toString(),
                                  data.value("reason").toString()));
      }
      else if (event == "render_item_failed")
      {
         Q_EMIT self_->ErrorOccurred(QString("%1: %2").arg(
            data.value("product").toString(), data.value("error").toString()));
      }
      else if (event == "render_complete")
      {
         if (!frames_.isEmpty())
         {
            SelectHour(frames_.back().forecastHour_);
         }
      }
      else if (event == "sounding_started")
      {
         Q_EMIT self_->ProgressUpdated("sounding", 0, 1, "Generating sounding");
      }
      else if (event == "sounding_complete")
      {
         auto sounding = types::ParseModelSounding(data, error);
         if (error.isEmpty())
         {
            Q_EMIT self_->ProgressUpdated(
               "sounding", 1, 1, "Sounding generated");
            Q_EMIT self_->SoundingAvailable(sounding);
         }
      }
      else if (event == "cache_status")
      {
         Q_EMIT self_->CacheStatusUpdated(data.value("bytes").toInteger(),
                                          data.value("files").toInteger());
      }

      if (!error.isEmpty())
      {
         Q_EMIT self_->ErrorOccurred(error);
      }
   }

   void ProcessFinished(int exitCode, QProcess::ExitStatus status)
   {
      ReadStandardOutput();
      const QString operationName = OperationName();
      const bool    failed = !cancelled_ && (status != QProcess::NormalExit ||
                                          exitCode != EXIT_SUCCESS);
      QString       finalStatus;
      if (failed)
      {
         finalStatus = QString::fromUtf8(stderrBuffer_).trimmed();
         if (finalStatus.isEmpty())
         {
            finalStatus = QString("Model %1 failed with exit code %2")
                             .arg(operationName)
                             .arg(exitCode);
         }
         Q_EMIT self_->ErrorOccurred(finalStatus);
      }
      else
      {
         finalStatus = cancelled_ ? QString("Cancelled %1").arg(operationName) :
                                    QString("%1 updated").arg(operationName);
      }
      Q_EMIT self_->StatusUpdated(finalStatus, QDateTime::currentDateTime());
      operation_ = Operation::None;
      Q_EMIT self_->OperationStateChanged(false, {});
   }

   void SelectHour(int hour)
   {
      QVector<types::ModelFrame> selected;
      std::copy_if(frames_.cbegin(),
                   frames_.cend(),
                   std::back_inserter(selected),
                   [this, hour](const auto& frame)
                   {
                      return frame.model_ == currentModel_ &&
                             frame.run_ == currentRun_ &&
                             frame.forecastHour_ == hour;
                   });
      Q_EMIT self_->FramesSelected(selected);
      if (!selected.isEmpty())
      {
         Q_EMIT self_->FrameSelected(selected.back());
      }
   }

   void SelectNearestTime(std::chrono::system_clock::time_point time)
   {
      if (frames_.isEmpty())
         return;
      const auto distance = [time](const auto& frame)
      {
         const auto delta = frame.validTime_ - time;
         return delta < decltype(delta)::zero() ? -delta : delta;
      };
      const auto match =
         std::min_element(frames_.cbegin(),
                          frames_.cend(),
                          [&distance](const auto& left, const auto& right)
                          { return distance(left) < distance(right); });
      if (match != frames_.cend())
         SelectHour(match->forecastHour_);
   }

   ModelManager*                 self_;
   QProcess                      process_;
   Operation                     operation_ {Operation::None};
   bool                          cancelled_ {false};
   QByteArray                    stdoutBuffer_;
   QByteArray                    stderrBuffer_;
   std::unique_ptr<QSettings>    settings_;
   QString                       bridgePath_;
   QString                       storeRoot_;
   QString                       cacheRoot_;
   QString                       outputRoot_;
   QSet<QString>                 favorites_;
   float                         opacity_ {0.8f};
   bool                          visible_ {true};
   bool                          timelineSync_ {true};
   QVector<types::ForecastModel> models_;
   QVector<types::ModelFrame>    frames_;
   QString                       currentModel_;
   QString                       currentRun_;
};

ModelManager::ModelManager() : p(std::make_unique<Impl>(this)) {}
ModelManager::~ModelManager() = default;

std::shared_ptr<ModelManager> ModelManager::Instance()
{
   static std::weak_ptr<ModelManager> reference;
   auto                               manager = reference.lock();
   if (!manager)
   {
      manager   = std::make_shared<ModelManager>();
      reference = manager;
   }
   return manager;
}

QString ModelManager::bridge_path() const
{
   return p->bridgePath_;
}
QString ModelManager::store_root() const
{
   return p->storeRoot_;
}
QString ModelManager::cache_root() const
{
   return p->cacheRoot_;
}
bool ModelManager::busy() const
{
   return p->process_.state() != QProcess::NotRunning;
}
QVector<types::ForecastModel> ModelManager::models() const
{
   return p->models_;
}
QSet<QString> ModelManager::favorites() const
{
   return p->favorites_;
}
float ModelManager::opacity() const
{
   return p->opacity_;
}
bool ModelManager::visible() const
{
   return p->visible_;
}
bool ModelManager::timeline_sync() const
{
   return p->timelineSync_;
}

void ModelManager::SetBridgePath(const QString& path)
{
   p->bridgePath_ = QDir::cleanPath(path);
   p->SaveSettings();
}

void ModelManager::SetFavorite(const QString& product, bool favorite)
{
   if (favorite)
      p->favorites_.insert(product);
   else
      p->favorites_.remove(product);
   p->SaveSettings();
}

void ModelManager::SetOpacity(float opacity)
{
   p->opacity_ = std::clamp(opacity, 0.0f, 1.0f);
   p->SaveSettings();
   Q_EMIT OpacityChanged(p->opacity_);
}

void ModelManager::SetVisible(bool visible)
{
   p->visible_ = visible;
   p->SaveSettings();
   Q_EMIT VisibilityChanged(visible);
}

void ModelManager::SetTimelineSync(bool enabled)
{
   p->timelineSync_ = enabled;
   p->SaveSettings();
}

void ModelManager::SelectForecastHour(int hour)
{
   p->SelectHour(hour);
}

void ModelManager::LoadCapabilities()
{
   p->Start(Impl::Operation::Capabilities, {"capabilities"});
}

void ModelManager::Probe(const QString& model,
                         const QDate&   date,
                         int            forecastHour,
                         const QString& source)
{
   QStringList args {"probe",
                     "--model",
                     model,
                     "--date",
                     date.toString("yyyyMMdd"),
                     "--forecast-hour",
                     QString::number(forecastHour)};
   if (!source.isEmpty())
      args.append({"--source", source});
   p->Start(Impl::Operation::Probe, args);
}

void ModelManager::Fetch(const QString& model,
                         const QString& date,
                         int            cycleUtc,
                         const QString& hours,
                         const QString& source,
                         const QString& profile,
                         bool           heavy,
                         bool           verify)
{
   QStringList args {"fetch",
                     "--model",
                     model,
                     "--date",
                     date,
                     "--cycle",
                     QString::number(cycleUtc),
                     "--hours",
                     hours,
                     "--store-root",
                     p->storeRoot_,
                     "--cache-dir",
                     p->cacheRoot_,
                     "--profile",
                     profile};
   if (!source.isEmpty())
      args.append({"--source", source});
   if (heavy)
      args.append("--heavy");
   if (verify)
      args.append("--verify");
   p->Start(Impl::Operation::Fetch, args);
}

void ModelManager::Catalog(const QString& model,
                           const QString& run,
                           int            forecastHour)
{
   p->Start(Impl::Operation::Catalog,
            {"catalog",
             "--store-root",
             p->storeRoot_,
             "--model",
             model,
             "--run",
             run,
             "--hour",
             QString::number(forecastHour)});
}

void ModelManager::ListRuns(const QString& model)
{
   p->Start(Impl::Operation::Runs,
            {"runs", "--store-root", p->storeRoot_, "--model", model});
}

void ModelManager::Render(const QString&     model,
                          const QString&     run,
                          const QString&     hours,
                          const QStringList& products,
                          double             west,
                          double             east,
                          double             south,
                          double             north,
                          int                width,
                          const QString&     source)
{
   const QString bounds = QString("%1,%2,%3,%4")
                             .arg(west, 0, 'f', 6)
                             .arg(east, 0, 'f', 6)
                             .arg(south, 0, 'f', 6)
                             .arg(north, 0, 'f', 6);
   const QString output =
      QDir(p->outputRoot_).filePath(QString("%1/%2").arg(model, run));
   QStringList args {"render",
                     "--store-root",
                     p->storeRoot_,
                     "--model",
                     model,
                     "--run",
                     run,
                     "--hours",
                     hours,
                     "--products",
                     products.join(','),
                     QString("--bounds=%1").arg(bounds),
                     "--out-dir",
                     output,
                     "--width",
                     QString::number(width)};
   if (!source.isEmpty())
      args.append({"--source", source});
   const QString previousModel = p->currentModel_;
   const QString previousRun   = p->currentRun_;
   p->currentModel_            = model;
   p->currentRun_              = run;
   if (p->Start(Impl::Operation::Render, args))
   {
      p->frames_.clear();
      Q_EMIT FramesSelected({});
   }
   else
   {
      p->currentModel_ = previousModel;
      p->currentRun_   = previousRun;
   }
}

void ModelManager::Sounding(const QString& model,
                            const QString& run,
                            int            forecastHour,
                            double         latitude,
                            double         longitude)
{
   const QString outputDirectory =
      QDir(p->outputRoot_).filePath(QString("%1/%2/soundings").arg(model, run));
   const QString outputFilename = QString("f%1_%2_%3.png")
                                     .arg(forecastHour, 3, 10, QChar('0'))
                                     .arg(latitude, 0, 'f', 4)
                                     .arg(longitude, 0, 'f', 4);
   const QString output = QDir(outputDirectory).filePath(outputFilename);
   p->Start(Impl::Operation::Sounding,
            {"sounding",
             "--store-root",
             p->storeRoot_,
             "--model",
             model,
             "--run",
             run,
             "--hour",
             QString::number(forecastHour),
             QString("--lat=%1").arg(latitude, 0, 'f', 6),
             QString("--lon=%1").arg(longitude, 0, 'f', 6),
             "--out",
             output});
}

void ModelManager::CacheStatus()
{
   p->Start(Impl::Operation::CacheStatus,
            {"cache-status", "--path", p->cacheRoot_});
}

void ModelManager::Cancel()
{
   if (p->process_.state() != QProcess::NotRunning)
   {
      p->cancelled_ = true;
      p->process_.terminate();
      QTimer::singleShot(1500,
                         this,
                         [this]()
                         {
                            if (p->process_.state() != QProcess::NotRunning)
                               p->process_.kill();
                         });
   }
}

} // namespace scwx::qt::manager
