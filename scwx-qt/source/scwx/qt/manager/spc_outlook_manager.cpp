#include <scwx/qt/manager/spc_outlook_manager.hpp>
#include <scwx/spc/spc_outlook_provider.hpp>
#include <scwx/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::spc_outlook_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr auto kAutoRefreshIntervalMs_ = std::chrono::minutes(10);

class SpcOutlookManager::Impl
{
public:
   explicit Impl(SpcOutlookManager* self) : self_(self)
   {
      refreshTimer_.setInterval(kAutoRefreshIntervalMs_);
      QObject::connect(&refreshTimer_,
                       &QTimer::timeout,
                       self_,
                       &SpcOutlookManager::OnRefreshTimer);
   }

   ~Impl()
   {
      fetchCancelled_.store(true);
      threadPool_.join();
   }

   void FetchOutlookSync(scwx::spc::OutlookDay     day,
                         scwx::spc::OutlookProduct product)
   {
      auto result = scwx::spc::SpcOutlookProvider::FetchOutlook(day, product);

      if (result.has_value())
      {
         {
            const std::lock_guard<std::mutex> lock(dataMutex_);
            data_ = std::make_shared<scwx::spc::OutlookData>(
               std::move(result.value()));
         }
         logger_->info("SPC outlook data updated: {} polygons",
                       data_->polygons_.size());

         QMetaObject::invokeMethod(
            self_,
            [this]()
            {
               if (!fetchCancelled_.load())
               {
                  Q_EMIT self_->OutlookDataUpdated();
               }
            },
            Qt::QueuedConnection);
      }
      else
      {
         std::string errorMsg = "Failed to fetch SPC outlook: " +
                                std::string(result.error().message());
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

   SpcOutlookManager* self_;

   QTimer            refreshTimer_;
   std::atomic<bool> autoRefresh_ {false};
   std::atomic<bool> fetchCancelled_ {false};

   mutable std::mutex                      dataMutex_;
   std::shared_ptr<scwx::spc::OutlookData> data_;

   scwx::spc::OutlookDay     selectedDay_ {scwx::spc::OutlookDay::Day1};
   scwx::spc::OutlookProduct selectedProduct_ {
      scwx::spc::OutlookProduct::Categorical};
   int opacity_ {70};

   boost::asio::thread_pool threadPool_ {1u};
};

SpcOutlookManager::SpcOutlookManager() :
    QObject(nullptr), p(std::make_unique<Impl>(this))
{
}
SpcOutlookManager::~SpcOutlookManager() = default;

void SpcOutlookManager::SelectDay(scwx::spc::OutlookDay day)
{
   if (p->selectedDay_ == day)
   {
      return;
   }

   p->selectedDay_ = day;
   FetchOutlookAsync(day, p->selectedProduct_);
}

void SpcOutlookManager::SelectProduct(scwx::spc::OutlookProduct product)
{
   if (p->selectedProduct_ == product)
   {
      return;
   }

   p->selectedProduct_ = product;
   FetchOutlookAsync(p->selectedDay_, product);
}

void SpcOutlookManager::SetOpacity(int opacity)
{
   p->opacity_ = opacity;
   // Layer picks up opacity from the manager on next render
   Q_EMIT OutlookDataUpdated();
}

void SpcOutlookManager::SetAutoRefresh(bool enabled)
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

void SpcOutlookManager::RefreshNow()
{
   FetchOutlookAsync(p->selectedDay_, p->selectedProduct_);
}

scwx::spc::OutlookDay SpcOutlookManager::GetSelectedDay() const
{
   return p->selectedDay_;
}

scwx::spc::OutlookProduct SpcOutlookManager::GetSelectedProduct() const
{
   return p->selectedProduct_;
}

int SpcOutlookManager::GetOpacity() const
{
   return p->opacity_;
}

bool SpcOutlookManager::IsAutoRefreshEnabled() const
{
   return p->autoRefresh_;
}

std::shared_ptr<scwx::spc::OutlookData>
SpcOutlookManager::GetOutlookData() const
{
   const std::lock_guard<std::mutex> lock(p->dataMutex_);
   return p->data_;
}

void SpcOutlookManager::FetchOutlookAsync(scwx::spc::OutlookDay     day,
                                          scwx::spc::OutlookProduct product)
{
   p->fetchCancelled_.store(false);

   boost::asio::post(p->threadPool_,
                     [this, day, product]()
                     { p->FetchOutlookSync(day, product); });
}

void SpcOutlookManager::OnRefreshTimer()
{
   RefreshNow();
}

SpcOutlookManager& SpcOutlookManager::Instance()
{
   static SpcOutlookManager instance_;
   return instance_;
}

} // namespace scwx::qt::manager
