#include <scwx/qt/view/overlay_product_view.hpp>
#include <scwx/qt/map/overlay_product_symbols.hpp>
#include <scwx/qt/manager/radar_product_manager.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <mutex>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/uuid/random_generator.hpp>

namespace scwx::qt::view
{

static const std::string logPrefix_ = "scwx::qt::view::overlay_product_view";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class OverlayProductView::Impl
{
public:
   explicit Impl(OverlayProductView* self) : self_ {self} {}
   ~Impl() = default;

   Impl(const Impl&)                = delete;
   Impl& operator=(const Impl&)     = delete;
   Impl(Impl&&) noexcept            = delete;
   Impl& operator=(Impl&&) noexcept = delete;

   void ConnectRadarProductManager();
   void DisconnectRadarProductManager();
   void LoadProduct(const std::string&                    product,
                    std::chrono::system_clock::time_point time,
                    bool                                  autoUpdate);
   void ResetProducts();
   void Update(const std::string& product);
   void UpdateAll();
   void UpdateAutoRefresh(bool enabled) const;

   OverlayProductView* self_;
   std::unordered_map<std::string, boost::uuids::uuid> uuidMap_ {};

   boost::uuids::uuid UuidFor(const std::string& product)
   {
      auto it = uuidMap_.find(product);
      if (it == uuidMap_.end())
      {
         it = uuidMap_
                 .emplace(product, boost::uuids::random_generator()())
                 .first;
      }
      return it->second;
   }

   boost::asio::thread_pool threadPool_ {1u};

   bool autoRefreshEnabled_ {false};
   bool autoUpdateEnabled_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::shared_ptr<manager::RadarProductManager> radarProductManager_ {nullptr};

   std::unordered_map<std::string, std::shared_ptr<wsr88d::rpg::Level3Message>>
              messageMap_ {};
   std::mutex messageMutex_ {};
};

OverlayProductView::OverlayProductView() : p(std::make_unique<Impl>(this)) {};
OverlayProductView::~OverlayProductView()
{
   // Stop callbacks that can post work before joining the pool, while Impl and
   // the QObject are still alive.
   p->DisconnectRadarProductManager();

   p->threadPool_.stop();
   p->threadPool_.join();
}

std::shared_ptr<manager::RadarProductManager>
OverlayProductView::radar_product_manager() const
{
   return p->radarProductManager_;
}

std::shared_ptr<wsr88d::rpg::Level3Message>
OverlayProductView::radar_product_message(const std::string& product) const
{
   std::unique_lock lock {p->messageMutex_};

   auto it = p->messageMap_.find(product);
   if (it != p->messageMap_.cend())
   {
      return it->second;
   }

   return nullptr;
}

std::chrono::system_clock::time_point OverlayProductView::selected_time() const
{
   return p->selectedTime_;
}

void OverlayProductView::set_radar_product_manager(
   const std::shared_ptr<manager::RadarProductManager>& radarProductManager)
{
   p->UpdateAutoRefresh(false);
   p->DisconnectRadarProductManager();
   p->ResetProducts();

   p->radarProductManager_ = radarProductManager;

   p->ConnectRadarProductManager();
   p->UpdateAutoRefresh(p->autoRefreshEnabled_);
}

void OverlayProductView::Impl::ConnectRadarProductManager()
{
   connect(radarProductManager_.get(),
           &manager::RadarProductManager::DataReloaded,
           self_,
           [this](std::shared_ptr<types::RadarProductRecord> record)
           {
              if (record->radar_product_group() !=
                     common::RadarProductGroup::Level3 ||
                  std::chrono::floor<std::chrono::seconds>(record->time()) !=
                     selectedTime_)
              {
                 return;
              }

              if (map::IsOverlayProduct(record->radar_product()) ||
                  map::IsOverlayProductCode(record->product_code()))
              {
                 Update(record->radar_product());
              }
           });

   connect(
      radarProductManager_.get(),
      &manager::RadarProductManager::NewDataAvailable,
      self_,
      [this](common::RadarProductGroup group,
             const std::string&        product,
             bool /*isChunks*/,
             std::chrono::system_clock::time_point latestTime)
      {
         if (autoRefreshEnabled_ &&
             group == common::RadarProductGroup::Level3 &&
             map::IsOverlayProduct(product))
         {
            LoadProduct(product, latestTime, autoUpdateEnabled_);
         }
      },
      Qt::QueuedConnection);

   connect(radarProductManager_.get(),
           &manager::RadarProductManager::ProductTimesPopulated,
           self_,
           [this](common::RadarProductGroup             group,
                  const std::string&                    product,
                  std::chrono::system_clock::time_point queryTime)
           {
              if (group == common::RadarProductGroup::Level3 &&
                  map::IsOverlayProduct(product) && queryTime == selectedTime_)
              {
                 Update(product);
              }
           });
}

void OverlayProductView::Impl::DisconnectRadarProductManager()
{
   if (radarProductManager_ != nullptr)
   {
      disconnect(radarProductManager_.get(),
                 &manager::RadarProductManager::DataReloaded,
                 self_,
                 nullptr);
      disconnect(radarProductManager_.get(),
                 &manager::RadarProductManager::NewDataAvailable,
                 self_,
                 nullptr);
      disconnect(radarProductManager_.get(),
                 &manager::RadarProductManager::ProductTimesPopulated,
                 self_,
                 nullptr);
   }
}

void OverlayProductView::Impl::LoadProduct(
   const std::string&                    product,
   std::chrono::system_clock::time_point time,
   bool                                  autoUpdate)
{
   logger_->debug(
      "Load Product: {}, {}, {}", product, util::TimeString(time), autoUpdate);

   // Create file request
   std::shared_ptr<request::NexradFileRequest> request =
      std::make_shared<request::NexradFileRequest>(
         radarProductManager_->radar_id());

   if (autoUpdate)
   {
      connect(
         request.get(),
         &request::NexradFileRequest::RequestComplete,
         self_,
         [=, this](std::shared_ptr<request::NexradFileRequest> request)
         {
            using namespace std::chrono_literals;

            // Select loaded record
            const auto& record = request->radar_product_record();

            std::shared_ptr<wsr88d::Level3File>         level3File = nullptr;
            std::shared_ptr<wsr88d::rpg::Level3Message> message    = nullptr;

            if (record != nullptr)
            {
               level3File = record->level3_file();
            }
            if (level3File != nullptr)
            {
               message = level3File->message();
            }

            // Validate record
            if (message != nullptr)
            {
               const auto& header      = message->header();
               auto        productTime = util::TimePoint(
                  header.date_of_message(), header.time_of_message() * 1000);

               // If the record is from the last 30 minutes
               if (productTime + 30min >= scwx::util::time::now() ||
                   (selectedTime_ != std::chrono::system_clock::time_point {} &&
                    productTime + 30min >= selectedTime_))
               {
                  // Store loaded record
                  std::unique_lock lock {messageMutex_};

                  auto it = messageMap_.find(product);
                  if (it == messageMap_.cend() || it->second != message)
                  {
                     messageMap_.insert_or_assign(product, message);

                     lock.unlock();

                     Q_EMIT self_->ProductUpdated(product);
                  }
               }
               else
               {
                  // If product is more than 30 minutes old, discard
                  std::unique_lock lock {messageMutex_};
                  std::size_t      elementsRemoved = messageMap_.erase(product);
                  lock.unlock();

                  if (elementsRemoved > 0)
                  {
                     Q_EMIT self_->ProductUpdated(product);
                  }

                  logger_->trace("Discarding stale data: {}",
                                 util::TimeString(productTime));
               }
            }
            else
            {
               // If the product doesn't exist, erase the stale product
               std::unique_lock lock {messageMutex_};
               std::size_t      elementsRemoved = messageMap_.erase(product);
               lock.unlock();

               if (elementsRemoved > 0)
               {
                  Q_EMIT self_->ProductUpdated(product);
               }

               logger_->trace("Removing stale product");
            }
         });
   }

   // Load file
   boost::asio::post(threadPool_,
                     [=, this]()
                     {
                        try
                        {
                           radarProductManager_->LoadLevel3Data(
                              product, time, request);
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void OverlayProductView::Impl::ResetProducts()
{
   std::unique_lock lock {messageMutex_};
   messageMap_.clear();
   lock.unlock();

   for (const auto& product : map::OverlayProductNames())
   {
      Q_EMIT self_->ProductUpdated(product);
   }
}

void OverlayProductView::SelectTime(std::chrono::system_clock::time_point time)
{
   if (time != p->selectedTime_)
   {
      p->selectedTime_ = time;
      if (p->radarProductManager_ != nullptr)
      {
         p->UpdateAll();
      }
   }
}

void OverlayProductView::SetAutoRefresh(bool enabled)
{
   if (p->autoRefreshEnabled_ != enabled)
   {
      p->autoRefreshEnabled_ = enabled;
      p->UpdateAutoRefresh(enabled);
   }
}

void OverlayProductView::Impl::Update(const std::string& product)
{
   if (radarProductManager_ == nullptr)
   {
      return;
   }

   // Retrieve message from Radar Product Manager
   std::shared_ptr<wsr88d::rpg::Level3Message> message;
   std::chrono::system_clock::time_point       requestedTime {selectedTime_};
   std::chrono::system_clock::time_point       foundTime;
   std::tie(message, foundTime, std::ignore) =
      radarProductManager_->GetLevel3Data(product, requestedTime);

   // If a different time was found than what was requested, update it
   if (requestedTime != foundTime)
   {
      selectedTime_ = foundTime;
   }

   if (message == nullptr)
   {
      logger_->debug("{} data not found", product);

      std::unique_lock lock {messageMutex_};
      const std::size_t elementsRemoved = messageMap_.erase(product);
      lock.unlock();

      if (elementsRemoved > 0)
      {
         Q_EMIT self_->ProductUpdated(product);
      }

      return;
   }

   std::unique_lock lock {messageMutex_};

   // Update message in map
   auto it = messageMap_.find(product);
   if (it == messageMap_.cend() || it->second != message)
   {
      messageMap_.insert_or_assign(product, message);

      lock.unlock();

      Q_EMIT self_->ProductUpdated(product);
   }
}

void OverlayProductView::Impl::UpdateAll()
{
   for (const auto& product : map::OverlayProductNames())
   {
      Update(product);
   }
}

void OverlayProductView::Impl::UpdateAutoRefresh(bool enabled) const
{
   if (radarProductManager_ == nullptr)
   {
      return;
   }

   for (const auto& product : map::OverlayProductNames())
   {
      radarProductManager_->EnableRefresh(common::RadarProductGroup::Level3,
                                          product,
                                          enabled,
                                          UuidFor(product));
   }
}

void OverlayProductView::SetAutoUpdate(bool enabled)
{
   p->autoUpdateEnabled_ = enabled;
}

} // namespace scwx::qt::view
