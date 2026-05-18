#include <scwx/qt/manager/spc_md_manager.hpp>
#include <scwx/spc/spc_md_provider.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::spc_md_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr auto kAutoRefreshIntervalMs_ = std::chrono::minutes(2);

class SpcMdManager::Impl
{
public:
   explicit Impl(SpcMdManager* self) : self_(self)
   {
      refreshTimer_.setInterval(kAutoRefreshIntervalMs_);
      QObject::connect(&refreshTimer_,
                       &QTimer::timeout,
                       self_,
                       &SpcMdManager::OnRefreshTimer);
   }

   ~Impl()
   {
      fetchCancelled_.store(true);
      threadPool_.join();
   }

   void FetchMdSync(uint64_t fetchId)
   {
      auto result = scwx::spc::SpcMdProvider::FetchActiveMDs();

      if (fetchId != currentFetchId_.load())
      {
         logger_->debug("Discarding stale SPC MD fetch result (ID: {})",
                        fetchId);
         return;
      }

      if (result.has_value())
      {
         {
            const std::lock_guard<std::mutex> lock(dataMutex_);
            data_ =
               std::make_shared<scwx::spc::MdData>(std::move(result.value()));
         }
         logger_->info("SPC MD data updated: {} discussions, emitting signal",
                       data_->discussions_.size());

         QMetaObject::invokeMethod(
            self_,
            [this]()
            {
               if (!fetchCancelled_.load())
               {
                  Q_EMIT self_->MdDataUpdated();
               }
            },
            Qt::QueuedConnection);
      }
      else
      {
         std::string errorMsg =
            "Failed to fetch SPC MD: " + std::string(result.error().message());
         logger_->warn(errorMsg);

         QMetaObject::invokeMethod(
            self_,
            [this, errorMsg]()
            {
               if (!fetchCancelled_.load())
               {
                  Q_EMIT self_->FetchError(QString::fromStdString(errorMsg));
               }
            },
            Qt::QueuedConnection);
      }
   }

   SpcMdManager* self_;

   QTimer                refreshTimer_;
   std::atomic<bool>     autoRefresh_ {false};
   std::atomic<bool>     fetchCancelled_ {false};
   std::atomic<uint64_t> currentFetchId_ {0};

   mutable std::mutex                 dataMutex_;
   std::shared_ptr<scwx::spc::MdData> data_;

   boost::asio::thread_pool threadPool_ {1u};
};

SpcMdManager::SpcMdManager() : QObject(nullptr), p(std::make_unique<Impl>(this))
{
}
SpcMdManager::~SpcMdManager() = default;

void SpcMdManager::SetAutoRefresh(bool enabled)
{
   p->autoRefresh_ = enabled;
   if (enabled)
   {
      p->refreshTimer_.start();
   }
   else
   {
      p->refreshTimer_.stop();
   }
}

void SpcMdManager::RefreshNow()
{
   FetchMdAsync();
}

std::shared_ptr<scwx::spc::MdData> SpcMdManager::GetMdData() const
{
   const std::lock_guard<std::mutex> lock(p->dataMutex_);
   return p->data_;
}

void SpcMdManager::FetchMdAsync()
{
   p->fetchCancelled_.store(false);
   uint64_t fetchId = ++p->currentFetchId_;
   logger_->info("Fetching MD data (fetch ID: {})", fetchId);

   boost::asio::post(p->threadPool_,
                     [this, fetchId]() { p->FetchMdSync(fetchId); });
}

void SpcMdManager::OnRefreshTimer()
{
   RefreshNow();
}

SpcMdManager& SpcMdManager::Instance()
{
   static SpcMdManager instance_;
   return instance_;
}

} // namespace scwx::qt::manager
