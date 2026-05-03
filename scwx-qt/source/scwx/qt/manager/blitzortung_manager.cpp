#include <scwx/qt/manager/blitzortung_manager.hpp>
#include <scwx/provider/blitzortung_provider.hpp>
#include <scwx/util/logger.hpp>

#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::blitzortung_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr auto kStrikeLifetimeMs_   = std::chrono::milliseconds(4000);
static constexpr auto kUpdateIntervalMs_   = std::chrono::milliseconds(60);
static constexpr auto kKeyPruneIntervalMs_ = std::chrono::milliseconds(10000);
using TimedStrike                          = TimedStrikeData;

class BlitzortungManager::Impl
{
public:
   explicit Impl(BlitzortungManager* self) : self_(self)
   {
      provider_.SetStrikeCallback([this](const provider::StrikeData& strike)
                                  { OnNewStrike(strike); });

      updateTimer_.setInterval(kUpdateIntervalMs_);
      QObject::connect(&updateTimer_,
                       &QTimer::timeout,
                       self_,
                       &BlitzortungManager::OnTimerTick);
   }

   ~Impl() { provider_.Stop(); }

   void OnNewStrike(const provider::StrikeData& strike)
   {
      const std::lock_guard<std::mutex> lock(strikesMutex_);

      // Dedup key: lat*1000, lon*1000, timeNs/1e9
      std::string key =
         std::to_string(static_cast<int>(strike.latitude * 1000.0)) + "," +
         std::to_string(static_cast<int>(strike.longitude * 1000.0)) + "," +
         std::to_string(strike.time_ns / 1000000000LL);

      auto now = std::chrono::steady_clock::now();

      if (recentKeys_.contains(key))
      {
         return;
      }
      recentKeys_[key] = now;

      strikes_.push_back({strike, now});
   }

   std::vector<TimedStrikeData> GetActiveStrikes()
   {
      const std::lock_guard<std::mutex> lock(strikesMutex_);
      PruneKeys();

      auto                         now = std::chrono::steady_clock::now();
      std::vector<TimedStrikeData> active;

      for (const auto& ts : strikes_)
      {
         if ((now - ts.receiptTime_) < kStrikeLifetimeMs_)
         {
            active.push_back(ts);
         }
      }

      return active;
   }

   void PruneKeys()
   {
      auto now    = std::chrono::steady_clock::now();
      auto cutoff = now - kKeyPruneIntervalMs_;

      for (auto it = recentKeys_.begin(); it != recentKeys_.end();)
      {
         if (it->second < cutoff)
         {
            it = recentKeys_.erase(it);
         }
         else
         {
            ++it;
         }
      }

      // Also prune old strikes
      auto strikeCutoff = now - kStrikeLifetimeMs_;
      std::erase_if(strikes_,
                    [&](const TimedStrike& ts)
                    { return ts.receiptTime_ < strikeCutoff; });
   }

   BlitzortungManager* self_;

   QTimer updateTimer_;

   mutable std::mutex       strikesMutex_;
   std::vector<TimedStrike> strikes_;
   std::unordered_map<std::string, std::chrono::steady_clock::time_point>
      recentKeys_;

   scwx::provider::BlitzortungProvider provider_;
};

BlitzortungManager::BlitzortungManager() :
    QObject(nullptr), p(std::make_unique<Impl>(this))
{
}
BlitzortungManager::~BlitzortungManager() = default;

void BlitzortungManager::Start()
{
   if (p->provider_.IsActive())
   {
      return;
   }

   p->updateTimer_.start();
   p->provider_.Start();

   logger_->info("Blitzortung manager started");
}

void BlitzortungManager::Stop()
{
   if (!p->provider_.IsActive())
   {
      return;
   }

   p->updateTimer_.stop();
   p->provider_.Stop();

   {
      const std::lock_guard<std::mutex> lock(p->strikesMutex_);
      p->strikes_.clear();
      p->recentKeys_.clear();
   }

   logger_->info("Blitzortung manager stopped");
}

bool BlitzortungManager::IsActive() const
{
   return p->provider_.IsActive();
}

std::vector<TimedStrikeData> BlitzortungManager::GetActiveStrikes() const
{
   return p->GetActiveStrikes();
}

void BlitzortungManager::OnNewStrike(const provider::StrikeData& strike)
{
   p->OnNewStrike(strike);
}

void BlitzortungManager::OnTimerTick()
{
   // Emit signal so the layer can update
   Q_EMIT StrikesUpdated();
}

BlitzortungManager& BlitzortungManager::Instance()
{
   static BlitzortungManager instance_;
   return instance_;
}

} // namespace scwx::qt::manager
