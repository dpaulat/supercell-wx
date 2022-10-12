#include <scwx/qt/manager/text_event_manager.hpp>
#include <scwx/awips/pvtec.hpp>
#include <scwx/awips/text_product_file.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/threads.hpp>

#include <shared_mutex>
#include <unordered_map>

#include <boost/container_hash/hash.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::text_event_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

struct TextEventKey
{
   TextEventKey(const awips::PVtec& pvtec) :
       officeId_ {pvtec.office_id()},
       phenomenon_ {pvtec.phenomenon()},
       significance_ {pvtec.significance()},
       etn_ {pvtec.event_tracking_number()}
   {
   }

   bool operator==(const TextEventKey& o) const;

   std::string         officeId_;
   awips::Phenomenon   phenomenon_;
   awips::Significance significance_;
   int16_t             etn_;
};

template<class Key>
struct TextEventHash;

template<>
struct TextEventHash<TextEventKey>
{
   size_t operator()(const TextEventKey& x) const
   {
      size_t seed = 0;
      boost::hash_combine(seed, x.officeId_);
      boost::hash_combine(seed, x.phenomenon_);
      boost::hash_combine(seed, x.significance_);
      boost::hash_combine(seed, x.etn_);
      return seed;
   }
};

class TextEventManager::Impl
{
public:
   explicit Impl() : textEventMap_ {}, textEventMutex_ {} {}

   ~Impl() {}

   void HandleMessage(std::shared_ptr<awips::TextProductMessage> message);

   std::unordered_map<TextEventKey,
                      std::list<std::shared_ptr<awips::TextProductMessage>>,
                      TextEventHash<TextEventKey>>
                     textEventMap_;
   std::shared_mutex textEventMutex_;
};

TextEventManager::TextEventManager() : p(std::make_unique<Impl>()) {}
TextEventManager::~TextEventManager() = default;

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
   auto&        vtecString = segments[0]->header_->vtecString_;
   TextEventKey key {vtecString[0].pVtec_};
   auto         it = textEventMap_.find(key);

   if (it == textEventMap_.cend())
   {
      // If there was no matching event, add the message to a new event
      textEventMap_.emplace(key, std::list {message});
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
   };
}

TextEventManager& TextEventManager::Instance()
{
   static TextEventManager textEventManager_ {};
   return textEventManager_;
}

bool TextEventKey::operator==(const TextEventKey& o) const
{
   return (officeId_ == o.officeId_ && phenomenon_ == o.phenomenon_ &&
           significance_ == o.significance_ && etn_ == o.etn_);
}

} // namespace manager
} // namespace qt
} // namespace scwx
