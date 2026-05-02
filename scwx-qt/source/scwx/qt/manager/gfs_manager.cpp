#include <scwx/qt/manager/gfs_manager.hpp>
#include <scwx/provider/gfs_provider.hpp>
#include <scwx/util/logger.hpp>

#include <memory>
#include <thread>

#include <QMetaObject>
#include <QtGlobal>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::gfs_manager";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class GfsManagerImpl
{
public:
   explicit GfsManagerImpl() :
       provider_(std::make_unique<provider::GfsProvider>())
   {
   }
   ~GfsManagerImpl() = default;

   GfsManagerImpl(const GfsManagerImpl&)            = delete;
   GfsManagerImpl& operator=(const GfsManagerImpl&) = delete;
   GfsManagerImpl(GfsManagerImpl&&)                 = delete;
   GfsManagerImpl& operator=(GfsManagerImpl&&)      = delete;

   void
   RequestSounding(GfsManager* self, double lat, double lon, int cycle, int fhr)
   {
      logger_->info("GFS sounding requested: lat={}, lon={}, cycle={}, fhr={}",
                    lat,
                    lon,
                    cycle,
                    fhr);

      // Run on a background thread using detached std::thread.
      // std::async's future destructor blocks the calling (UI) thread
      // until the async operation completes, causing the app to freeze.
      std::thread(
         [self, this, lat, lon, cycle, fhr]()
         {
            auto result = provider_->FetchSounding(lat, lon, cycle, fhr);

            if (result.has_value())
            {
               auto sounding = std::move(result.value());
               logger_->info("GFS sounding loaded successfully ({} levels)",
                             sounding->levels().size());

               QMetaObject::invokeMethod(
                  self,
                  [self, sounding]() { Q_EMIT self->SoundingReady(sounding); },
                  Qt::QueuedConnection);
            }
            else
            {
               logger_->warn("GFS sounding failed");

               QMetaObject::invokeMethod(
                  self,
                  [self]()
                  {
                     Q_EMIT self->LoadError(QStringLiteral(
                        "Failed to fetch GFS sounding. "
                        "Check network connection and try again."));
                  },
                  Qt::QueuedConnection);
            }
         })
         .detach();
   }

   std::unique_ptr<provider::GfsProvider> provider_;
};

GfsManager::GfsManager() :
    QObject(nullptr), p(std::make_unique<GfsManagerImpl>())
{
}
GfsManager::~GfsManager() = default;

GfsManager& GfsManager::Instance()
{
   static GfsManager instance;
   return instance;
}

void GfsManager::RequestSounding(double lat, double lon, int cycle, int fhr)
{
   p->RequestSounding(this, lat, lon, cycle, fhr);
}

} // namespace scwx::qt::manager
