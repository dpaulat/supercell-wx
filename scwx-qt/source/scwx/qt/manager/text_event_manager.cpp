#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/main/application.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/awips/text_product_file.hpp>
#include <scwx/provider/iem_api_provider.ipp>
#include <scwx/provider/warnings_provider.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <algorithm>
#include <atomic>
#include <list>
#include <map>
#include <shared_mutex>
#include <unordered_map>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/container/stable_vector.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/transform.hpp>

#if (__cpp_lib_chrono < 201907L)
#   include <date/date.h>
#endif

namespace scwx::qt::manager
{

using namespace std::chrono_literals;

static const std::string logPrefix_ = "scwx::qt::manager::text_event_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::chrono::hours kInitialLoadHistoryDuration_ =
   std::chrono::days {3};
static constexpr std::chrono::hours kDefaultLoadHistoryDuration_ =
   std::chrono::hours {1};

static const std::array<std::string, 8> kPils_ = {
   "FFS", "FFW", "MWS", "SMW", "SQW", "SVR", "SVS", "TOR"};

static const std::
   unordered_map<std::string, std::pair<std::chrono::hours, std::chrono::hours>>
      kPilLoadWindows_ {{"FFS", {-24h, 1h}},
                        {"FFW", {-24h, 1h}},
                        {"MWS", {-4h, 1h}},
                        {"SMW", {-4h, 1h}},
                        {"SQW", {-4h, 1h}},
                        {"SVR", {-4h, 1h}},
                        {"SVS", {-4h, 1h}},
                        {"TOR", {-4h, 1h}}};

// Widest load window provided by kPilLoadWindows_
static const std::pair<std::chrono::hours, std::chrono::hours>
   kArchiveLoadWindow_ {-24h, 1h};

class TextEventManager::Impl
{
public:
   explicit Impl(TextEventManager* self) :
       self_ {self},
       refreshTimer_ {threadPool_},
       refreshMutex_ {},
       textEventMap_ {},
       textEventMutex_ {}
   {
      auto& generalSettings = settings::GeneralSettings::Instance();

      warningsProvider_ = std::make_shared<provider::WarningsProvider>(
         generalSettings.warnings_provider().GetValue());

      warningsProviderChangedCallbackUuid_ =
         generalSettings.warnings_provider().RegisterValueChangedCallback(
            [this](const std::string& value)
            {
               loadHistoryDuration_ = kInitialLoadHistoryDuration_;
               warningsProvider_ =
                  std::make_shared<provider::WarningsProvider>(value);
            });

      boost::asio::post(threadPool_,
                        [this]()
                        {
                           try
                           {
                              main::Application::WaitForInitialization();
                              logger_->debug("Start Refresh");
                              Refresh();
                           }
                           catch (const std::exception& ex)
                           {
                              logger_->error(ex.what());
                           }
                        });
   }

   ~Impl()
   {
      settings::GeneralSettings::Instance()
         .warnings_provider()
         .UnregisterValueChangedCallback(warningsProviderChangedCallbackUuid_);

      // Indicate we are stopping so no new timers/tasks are scheduled
      stopping_ = true;

      // Shut down the warnings provider
      warningsProvider_->Shutdown();

      // Cancel the refresh timer while holding the mutex so Refresh() can't
      // create a new timer concurrently.
      std::unique_lock lock(refreshMutex_);
      refreshTimer_.cancel();
      lock.unlock();

      // Stop the thread pool so no further posted handlers will be executed,
      // then join to wait for workers to exit.
      threadPool_.stop();
      threadPool_.join();
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void HandleMessage(const std::shared_ptr<awips::TextProductMessage>& message,
                      bool archiveEvent = false);
   template<ranges::forward_range DateRange>
      requires std::same_as<ranges::range_value_t<DateRange>,
                            std::chrono::sys_days>
   void ListArchives(DateRange dates);
   void LoadArchives(std::chrono::system_clock::time_point dateTime);
   void PruneArchives();
   void RefreshAsync();
   void Refresh();
   template<ranges::forward_range DateRange>
      requires std::same_as<ranges::range_value_t<DateRange>,
                            std::chrono::sys_days>
   void UpdateArchiveDates(DateRange dates);

   // Thread pool sized for:
   // - Live Refresh (1x)
   // - Archive Loading (1x)
   boost::asio::thread_pool threadPool_ {2u};

   TextEventManager* self_;

   std::atomic<bool> stopping_ {false};

   boost::asio::steady_timer refreshTimer_;
   std::mutex                refreshMutex_;

   std::unordered_map<types::TextEventKey,
                      std::vector<std::shared_ptr<awips::TextProductMessage>>,
                      types::TextEventHash<types::TextEventKey>>
                     textEventMap_;
   std::shared_mutex textEventMutex_;

   std::shared_ptr<provider::WarningsProvider> warningsProvider_ {nullptr};

   std::chrono::hours loadHistoryDuration_ {kInitialLoadHistoryDuration_};
   std::chrono::sys_time<std::chrono::hours> prevLoadTime_ {};
   std::chrono::sys_days                     archiveLimit_ {};

   std::mutex                       archiveMutex_ {};
   std::list<std::chrono::sys_days> archiveDates_ {};

   std::mutex archiveEventKeyMutex_ {};
   std::map<std::chrono::sys_days,
            std::unordered_set<types::TextEventKey,
                               types::TextEventHash<types::TextEventKey>>>
      archiveEventKeys_ {};
   std::unordered_set<types::TextEventKey,
                      types::TextEventHash<types::TextEventKey>>
      liveEventKeys_ {};

   std::mutex unloadedProductMapMutex_ {};
   std::map<std::chrono::sys_days,
            boost::container::stable_vector<scwx::types::iem::AfosEntry>>
      unloadedProductMap_;

   boost::uuids::uuid warningsProviderChangedCallbackUuid_ {};
};

TextEventManager::TextEventManager() : p(std::make_unique<Impl>(this)) {}
TextEventManager::~TextEventManager() = default;

size_t TextEventManager::message_count(const types::TextEventKey& key) const
{
   size_t messageCount = 0u;

   std::shared_lock lock(p->textEventMutex_);

   auto it = p->textEventMap_.find(key);
   if (it != p->textEventMap_.cend())
   {
      messageCount = it->second.size();
   }

   return messageCount;
}

std::vector<std::shared_ptr<awips::TextProductMessage>>
TextEventManager::message_list(const types::TextEventKey& key) const
{
   std::vector<std::shared_ptr<awips::TextProductMessage>> messageList {};

   std::shared_lock lock(p->textEventMutex_);

   auto it = p->textEventMap_.find(key);
   if (it != p->textEventMap_.cend())
   {
      messageList.assign(it->second.begin(), it->second.end());
   }

   return messageList;
}

void TextEventManager::LoadFile(const std::string& filename)
{
   logger_->debug("LoadFile: {}", filename);

   boost::asio::post(p->threadPool_,
                     [=, this]()
                     {
                        try
                        {
                           awips::TextProductFile file;

                           // Load file
                           bool fileLoaded = file.LoadFile(filename);
                           if (!fileLoaded)
                           {
                              return;
                           }

                           // Process messages
                           auto messages = file.messages();
                           for (auto& message : messages)
                           {
                              p->HandleMessage(message);
                           }
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void TextEventManager::SelectTime(
   std::chrono::system_clock::time_point dateTime)
{
   if (dateTime == std::chrono::system_clock::time_point {})
   {
      // Ignore a default date/time selection
      return;
   }

   logger_->trace("Select Time: {}", util::TimeString(dateTime));

   boost::asio::post(
      p->threadPool_,
      [dateTime, this]()
      {
         try
         {
            const auto today = std::chrono::floor<std::chrono::days>(dateTime);
            const auto yesterday = today - std::chrono::days {1};
            const auto tomorrow  = today + std::chrono::days {1};
            const auto dateArray = std::array {yesterday, today, tomorrow};

            const auto dates =
               dateArray |
               ranges::views::filter(
                  [this](const auto& date)
                  {
                     return p->archiveLimit_ == std::chrono::sys_days {} ||
                            date < p->archiveLimit_;
                  });

            const std::unique_lock lock {p->archiveMutex_};

            p->UpdateArchiveDates(dates);
            p->ListArchives(dates);
            p->LoadArchives(dateTime);
            p->PruneArchives();
         }
         catch (const std::exception& ex)
         {
            logger_->error(ex.what());
         }
      });
}

void TextEventManager::Impl::HandleMessage(
   const std::shared_ptr<awips::TextProductMessage>& message, bool archiveEvent)
{
   using namespace std::chrono_literals;

   auto segments = message->segments();

   // If there are no segments, skip this message
   if (segments.empty())
   {
      return;
   }

   for (auto& segment : segments)
   {
      // If a segment has no header, or if there is no VTEC string, skip this
      // message. A segmented message corresponding to a text event should have
      // this information.
      if (!segment->header_.has_value() ||
          segment->header_->vtecString_.empty())
      {
         return;
      }
   }

   // Determine year
   const std::chrono::year_month_day wmoDate =
      std::chrono::floor<std::chrono::days>(
         message->wmo_header()->GetDateTime());
   const std::chrono::year wmoYear = wmoDate.year();

   std::unique_lock lock(textEventMutex_);

   // Find a matching event in the event map
   auto&               vtecString = segments[0]->header_->vtecString_;
   types::TextEventKey key {vtecString[0].pVtec_, wmoYear};
   size_t              messageIndex = 0;
   auto                it           = textEventMap_.find(key);
   bool                updated      = false;

   if (
      // If there was no matching event
      it == textEventMap_.cend() &&
      // The event is not new
      vtecString[0].pVtec_.action() != awips::PVtec::Action::New &&
      // The message was on January 1
      wmoDate.month() == std::chrono::January && wmoDate.day() == 1d &&
      // This is at least the 10th ETN of the year
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers): Readability
      vtecString[0].pVtec_.event_tracking_number() > 10)
   {
      // Attempt to find a matching event from last year
      key = {vtecString[0].pVtec_, wmoYear - std::chrono::years {1}};
      it  = textEventMap_.find(key);
   }

   if (it == textEventMap_.cend())
   {
      // If there was no matching event, add the message to a new event
      textEventMap_.emplace(key, std::vector {message});
      messageIndex = 0;
      updated      = true;

      if (!archiveEvent)
      {
         // Add the Text Event Key to the list of live events to prevent pruning
         liveEventKeys_.insert(key);
      }
   }
   else if (std::find_if(it->second.cbegin(),
                         it->second.cend(),
                         [=](auto& storedMessage)
                         {
                            return *message->wmo_header().get() ==
                                   *storedMessage->wmo_header().get();
                         }) == it->second.cend())
   {
      // If there was a matching event, and this message has not been stored
      // (WMO header equivalence check), add the updated message to the existing
      // event

      // Determine the chronological sequence of the message. Note, if there
      // were no time hints given to the WMO header, this will place the message
      // at the end of the vector.
      auto insertionPoint = std::upper_bound(
         it->second.begin(),
         it->second.end(),
         message,
         [](const std::shared_ptr<awips::TextProductMessage>& a,
            const std::shared_ptr<awips::TextProductMessage>& b)
         {
            return a->wmo_header()->GetDateTime() <
                   b->wmo_header()->GetDateTime();
         });

      // Insert the message in chronological order
      messageIndex = std::distance(it->second.begin(), insertionPoint);
      it->second.insert(insertionPoint, message);
      updated = true;
   };

   // If this is an archive event, and the key does not exist in the live events
   // Assumption: A live event will always be loaded before a duplicate archive
   // event
   if (archiveEvent && !liveEventKeys_.contains(key))
   {
      // Add the Text Event Key to the current date's archive
      const std::unique_lock archiveEventKeyLock {archiveEventKeyMutex_};
      auto&                  archiveKeys = archiveEventKeys_[wmoDate];
      archiveKeys.insert(key);
   }

   lock.unlock();

   if (updated)
   {
      Q_EMIT self_->AlertUpdated(key, messageIndex, message->uuid());
   }
}

template<ranges::forward_range DateRange>
   requires std::same_as<ranges::range_value_t<DateRange>,
                         std::chrono::sys_days>
void TextEventManager::Impl::ListArchives(DateRange dates)
{
   // Don't reload data that has already been loaded
   auto filteredDates =
      dates |
      ranges::views::filter([this](const auto& date)
                            { return !unloadedProductMap_.contains(date); });

   const auto dv = ranges::to<std::vector>(filteredDates);

   std::for_each(
      std::execution::par,
      dv.begin(),
      dv.end(),
      [this](const auto& date)
      {
         static const auto kEmptyRange_ =
            ranges::views::single(std::string_view {});
         static const auto kPilsView_ =
            kPils_ |
            ranges::views::transform([](const std::string& pil)
                                     { return std::string_view {pil}; });

         const auto dateArray = std::array {date};

         auto productEntries = provider::IemApiProvider::ListTextProducts(
            dateArray | ranges::views::all, kEmptyRange_, kPilsView_);

         const std::unique_lock lock {unloadedProductMapMutex_};

         if (productEntries.has_value())
         {
            unloadedProductMap_.try_emplace(
               date,
               boost::container::stable_vector<scwx::types::iem::AfosEntry> {
                  std::make_move_iterator(productEntries.value().begin()),
                  std::make_move_iterator(productEntries.value().end())});
         }
      });
}

void TextEventManager::Impl::LoadArchives(
   std::chrono::system_clock::time_point dateTime)
{
   using namespace std::chrono;

#if (__cpp_lib_chrono >= 201907L)
   namespace df = std;

   static constexpr std::string_view kDateFormat {"{:%Y%m%d%H%M}"};
#else
   using namespace date;
   namespace df = date;

#   define kDateFormat "%Y%m%d%H%M"
#endif

   // Search unloaded products in the widest archive load window
   const std::chrono::sys_days startDate =
      std::chrono::floor<std::chrono::days>(dateTime +
                                            kArchiveLoadWindow_.first);
   const std::chrono::sys_days endDate = std::chrono::floor<std::chrono::days>(
      dateTime + kArchiveLoadWindow_.second + std::chrono::days {1});

   // Determine load windows for each PIL
   std::unordered_map<std::string, std::pair<std::string, std::string>>
      pilLoadWindowStrings;

   for (auto& loadWindow : kPilLoadWindows_)
   {
      const std::string& pil = loadWindow.first;

      pilLoadWindowStrings.insert_or_assign(
         pil,
         std::pair<std::string, std::string> {
            df::format(kDateFormat, (dateTime + loadWindow.second.first)),
            df::format(kDateFormat, (dateTime + loadWindow.second.second))});
   }

   std::vector<scwx::types::iem::AfosEntry> loadListEntries {};

   for (auto date = startDate; date < endDate; date += std::chrono::days {1})
   {
      auto mapIt = unloadedProductMap_.find(date);
      if (mapIt == unloadedProductMap_.cend())
      {
         continue;
      }

      for (auto it = mapIt->second.begin(); it != mapIt->second.end();)
      {
         const auto& pil = it->pil_;

         // Check PIL
         if (pil.size() >= 3)
         {
            auto pilPrefix = pil.substr(0, 3);
            auto windowIt  = pilLoadWindowStrings.find(pilPrefix);

            // Check Window
            if (windowIt != pilLoadWindowStrings.cend())
            {
               const auto& productId   = it->productId_;
               const auto& windowStart = windowIt->second.first;
               const auto& windowEnd   = windowIt->second.second;

               if (windowStart <= productId && productId <= windowEnd)
               {
                  // Product matches, move it to the load list
                  loadListEntries.emplace_back(std::move(*it));
                  it = mapIt->second.erase(it);
                  continue;
               }
            }
         }

         // Current iterator was not matched
         ++it;
      }
   }

   std::vector<std::shared_ptr<awips::TextProductFile>> products {};

   // Load the load list
   if (!loadListEntries.empty())
   {
      auto loadView = loadListEntries |
                      ranges::views::transform([](const auto& entry)
                                               { return entry.productId_; });
      products = provider::IemApiProvider::LoadTextProducts(loadView);
   }

   // Process loaded products
   for (auto& product : products)
   {
      const auto& messages = product->messages();

      for (auto& message : messages)
      {
         HandleMessage(message, true);
      }
   }
}

void TextEventManager::Impl::PruneArchives()
{
   static constexpr std::size_t kMaxArchiveDates_ = 5;

   std::unordered_set<types::TextEventKey,
                      types::TextEventHash<types::TextEventKey>>
      eventKeysToKeep {};
   std::unordered_set<types::TextEventKey,
                      types::TextEventHash<types::TextEventKey>>
      eventKeysToPrune {};

   // Remove oldest dates from the archive
   while (archiveDates_.size() > kMaxArchiveDates_)
   {
      archiveDates_.pop_front();
   }

   const std::unique_lock archiveEventKeyLock {archiveEventKeyMutex_};

   // If there are the same number of dates in archiveEventKeys_, archiveDates_
   // and unloadedProductMap_, there is nothing to prune
   if (archiveEventKeys_.size() == archiveDates_.size() &&
       unloadedProductMap_.size() == archiveDates_.size())
   {
      // Nothing to prune
      return;
   }

   const std::unique_lock unloadedProductMapLock {unloadedProductMapMutex_};

   for (auto it = archiveEventKeys_.begin(); it != archiveEventKeys_.end();)
   {
      const auto& date      = it->first;
      const auto& eventKeys = it->second;

      // If date is not in recent days map
      if (std::find(archiveDates_.cbegin(), archiveDates_.cend(), date) ==
          archiveDates_.cend())
      {
         // Prune these keys (unless they are in the eventKeysToKeep set)
         eventKeysToPrune.insert(eventKeys.begin(), eventKeys.end());

         // The date is not in the list of recent dates, remove it
         it = archiveEventKeys_.erase(it);
      }
      else
      {
         // Make sure these keys don't get pruned
         eventKeysToKeep.insert(eventKeys.begin(), eventKeys.end());

         // The date is recent, keep it
         ++it;
      }
   }

   for (auto it = unloadedProductMap_.begin(); it != unloadedProductMap_.end();)
   {
      const auto& date = it->first;

      // If date is not in recent days map
      if (std::find(archiveDates_.cbegin(), archiveDates_.cend(), date) ==
          archiveDates_.cend())
      {
         // The date is not in the list of recent dates, remove it
         it = unloadedProductMap_.erase(it);
      }
      else
      {
         // The date is recent, keep it
         ++it;
      }
   }

   // Remove elements from eventKeysToPrune if they are in eventKeysToKeep
   for (const auto& eventKey : eventKeysToKeep)
   {
      eventKeysToPrune.erase(eventKey);
   }

   // Remove eventKeysToPrune from textEventMap
   for (const auto& eventKey : eventKeysToPrune)
   {
      textEventMap_.erase(eventKey);
   }

   // If event keys were pruned, emit a signal
   if (!eventKeysToPrune.empty())
   {
      logger_->debug("Pruned {} archive events", eventKeysToPrune.size());

      Q_EMIT self_->AlertsRemoved(eventKeysToPrune);
   }
}

void TextEventManager::Impl::RefreshAsync()
{
   boost::asio::post(threadPool_,
                     [this]()
                     {
                        try
                        {
                           Refresh();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

void TextEventManager::Impl::Refresh()
{
   logger_->trace("Refresh");

   // Take a unique lock before refreshing
   std::unique_lock lock(refreshMutex_);

   // Take a copy of the current warnings provider to protect against change
   std::shared_ptr<provider::WarningsProvider> warningsProvider =
      warningsProvider_;

   // Load updated files from the warnings provider
   // Start time should default to:
   // - 3 days of history for the first load
   // - 1 hour of history for subsequent loads
   // If the time jumps, we should attempt to load from no later than the
   // previous load time
   auto loadTime =
      std::chrono::floor<std::chrono::hours>(scwx::util::time::now());
   auto startTime = loadTime - loadHistoryDuration_;

   if (prevLoadTime_ != std::chrono::sys_time<std::chrono::hours> {})
   {
      startTime = std::min(startTime, prevLoadTime_);
   }

   if (archiveLimit_ == std::chrono::sys_days {})
   {
      archiveLimit_ = std::chrono::ceil<std::chrono::days>(startTime);
   }

   auto updatedFiles = warningsProvider->LoadUpdatedFiles(startTime);

   // Store the load time and reset the load history duration
   prevLoadTime_        = loadTime;
   loadHistoryDuration_ = kDefaultLoadHistoryDuration_;

   // Handle messages
   for (auto& file : updatedFiles)
   {
      for (auto& message : file->messages())
      {
         HandleMessage(message);
      }
   }

   // Check for shutdown
   if (stopping_)
   {
      return;
   }

   // Schedule another update in 15 seconds
   using namespace std::chrono;
   refreshTimer_.expires_after(15s);
   refreshTimer_.async_wait(
      [this](const boost::system::error_code& e)
      {
         if (e == boost::asio::error::operation_aborted)
         {
            logger_->debug("Refresh timer cancelled");
         }
         else if (e != boost::system::errc::success)
         {
            logger_->warn("Refresh timer error: {}", e.message());
         }
         else
         {
            RefreshAsync();
         }
      });
}

template<ranges::forward_range DateRange>
   requires std::same_as<ranges::range_value_t<DateRange>,
                         std::chrono::sys_days>
void TextEventManager::Impl::UpdateArchiveDates(DateRange dates)
{
   for (const auto& date : dates)
   {
      // Remove any existing occurrences of day, and add to the back of the list
      archiveDates_.remove(date);
      archiveDates_.push_back(date);
   }
}

std::shared_ptr<TextEventManager> TextEventManager::Instance()
{
   static std::weak_ptr<TextEventManager> textEventManagerReference_ {};
   static std::mutex                      instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<TextEventManager> textEventManager =
      textEventManagerReference_.lock();

   if (textEventManager == nullptr)
   {
      textEventManager           = std::make_shared<TextEventManager>();
      textEventManagerReference_ = textEventManager;
   }

   return textEventManager;
}

} // namespace scwx::qt::manager
