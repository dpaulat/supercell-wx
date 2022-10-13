#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/awips/text_product_file.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <shared_mutex>
#include <unordered_map>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::text_event_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class TextEventManager::Impl
{
public:
   explicit Impl(TextEventManager* self) :
       self_ {self}, textEventMap_ {}, textEventMutex_ {}
   {
   }

   ~Impl() {}

   void HandleMessage(std::shared_ptr<awips::TextProductMessage> message);

   TextEventManager* self_;

   std::unordered_map<types::TextEventKey,
                      std::list<std::shared_ptr<awips::TextProductMessage>>,
                      types::TextEventHash<types::TextEventKey>>
                     textEventMap_;
   std::shared_mutex textEventMutex_;
};

TextEventManager::TextEventManager() : p(std::make_unique<Impl>(this)) {}
TextEventManager::~TextEventManager() = default;

std::list<std::shared_ptr<awips::TextProductMessage>>
TextEventManager::message_list(const types::TextEventKey& key) const
{
   std::list<std::shared_ptr<awips::TextProductMessage>> messageList {};

   std::shared_lock lock(p->textEventMutex_);

   auto it = p->textEventMap_.find(key);
   if (it != p->textEventMap_.cend())
   {
      messageList = it->second;
   }

   return messageList;
}

void TextEventManager::LoadFile(const std::string& filename)
{
   logger_->debug("LoadFile: {}", filename);

   util::async(
      [=]()
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

   // If there are no segments, if the first segment has no header, or if there
   // is no VTEC string, skip this message
   if (segments.empty() || !segments[0]->header_.has_value() ||
       segments[0]->header_->vtecString_.empty())
   {
      return;
   }

   std::unique_lock lock(textEventMutex_);

   // Find a matching event in the event map
   auto&               vtecString = segments[0]->header_->vtecString_;
   types::TextEventKey key {vtecString[0].pVtec_};
   auto                it      = textEventMap_.find(key);
   bool                updated = false;

   if (it == textEventMap_.cend())
   {
      // If there was no matching event, add the message to a new event
      textEventMap_.emplace(key, std::list {message});
      updated = true;
   }
   else if (std::find_if(it->second.cbegin(),
                         it->second.cend(),
                         [=](auto& storedMessage) {
                            return message->wmo_header() ==
                                   storedMessage->wmo_header();
                         }) == it->second.cend())
   {
      // If there was a matching event, and this message has not been stored
      // (WMO header equivalence check), add the updated message to the existing
      // event
      it->second.push_back(message);
      updated = true;
   };

   lock.unlock();

   if (updated)
   {
      emit self_->AlertUpdated(key);
   }
}

TextEventManager& TextEventManager::Instance()
{
   static TextEventManager textEventManager_ {};
   return textEventManager_;
}

} // namespace manager
} // namespace qt
} // namespace scwx
