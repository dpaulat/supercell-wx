#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/qt/main/application.hpp>
#include <scwx/awips/text_product_file.hpp>
#include <scwx/provider/warnings_provider.hpp>
#include <scwx/util/logger.hpp>

#include <shared_mutex>
#include <unordered_map>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::text_event_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string& kDefaultWarningsProviderUrl {
   "https://warnings.allisonhouse.com"};

class TextEventManager::Impl
{
public:
   explicit Impl(TextEventManager* self) :
       self_ {self},
       refreshTimer_ {threadPool_},
       refreshMutex_ {},
       textEventMap_ {},
       textEventMutex_ {},
       warningsProvider_ {kDefaultWarningsProviderUrl}
   {
      boost::asio::post(threadPool_,
                        [this]()
                        {
                           main::Application::WaitForInitialization();
                           logger_->debug("Start Refresh");
                           Refresh();
                        });
   }

   ~Impl()
   {
      std::unique_lock lock(refreshMutex_);
      refreshTimer_.cancel();
   }

   void HandleMessage(std::shared_ptr<awips::TextProductMessage> message);
   void Refresh();

   boost::asio::thread_pool threadPool_ {1u};

   TextEventManager* self_;

   boost::asio::steady_timer refreshTimer_;
   std::mutex                refreshMutex_;

   std::unordered_map<types::TextEventKey,
                      std::vector<std::shared_ptr<awips::TextProductMessage>>,
                      types::TextEventHash<types::TextEventKey>>
                     textEventMap_;
   std::shared_mutex textEventMutex_;

   provider::WarningsProvider warningsProvider_;
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
                     });
}

void TextEventManager::Impl::HandleMessage(
   std::shared_ptr<awips::TextProductMessage> message)
{
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

   std::unique_lock lock(textEventMutex_);

   // Find a matching event in the event map
   auto&               vtecString = segments[0]->header_->vtecString_;
   types::TextEventKey key {vtecString[0].pVtec_};
   size_t              messageIndex = 0;
   auto                it           = textEventMap_.find(key);
   bool                updated      = false;

   if (it == textEventMap_.cend())
   {
      // If there was no matching event, add the message to a new event
      textEventMap_.emplace(key, std::vector {message});
      messageIndex = 0;
      updated      = true;
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
      messageIndex = it->second.size();
      it->second.push_back(message);
      updated = true;
   };

   lock.unlock();

   if (updated)
   {
      Q_EMIT self_->AlertUpdated(key, messageIndex);
   }
}

void TextEventManager::Impl::Refresh()
{
   logger_->trace("Refresh");

   // Take a unique lock before refreshing
   std::unique_lock lock(refreshMutex_);

   // Update the file listing from the warnings provider
   auto [newFiles, totalFiles] = warningsProvider_.ListFiles();

   if (newFiles > 0)
   {
      // Load new files
      auto updatedFiles = warningsProvider_.LoadUpdatedFiles();

      // Handle messages
      for (auto& file : updatedFiles)
      {
         for (auto& message : file->messages())
         {
            HandleMessage(message);
         }
      }
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
            Refresh();
         }
      });
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

} // namespace manager
} // namespace qt
} // namespace scwx
