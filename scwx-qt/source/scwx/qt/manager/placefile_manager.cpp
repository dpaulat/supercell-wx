#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/gr/placefile.hpp>
#include <scwx/util/logger.hpp>

#include <shared_mutex>
#include <vector>

#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::placefile_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileRecord
{
public:
   explicit PlacefileRecord(const std::string&             name,
                            std::shared_ptr<gr::Placefile> placefile,
                            bool                           enabled = true) :
       name_ {name}, placefile_ {placefile}, enabled_ {enabled}
   {
   }
   ~PlacefileRecord()
   {
      std::unique_lock lock(refreshMutex_);
      refreshTimer_.cancel();
   }

   void Update(std::shared_ptr<gr::Placefile> placefile);

   std::string                    name_;
   std::shared_ptr<gr::Placefile> placefile_;
   bool                           enabled_;
   boost::asio::thread_pool       threadPool_ {1u};
   boost::asio::steady_timer      refreshTimer_ {threadPool_};
   std::mutex                     refreshMutex_ {};
};

class PlacefileManager::Impl
{
public:
   explicit Impl(PlacefileManager* self) : self_ {self} {}
   ~Impl() {}

   boost::asio::thread_pool threadPool_ {1u};

   PlacefileManager* self_;

   std::vector<std::unique_ptr<PlacefileRecord>> placefileRecords_ {};
   std::shared_mutex                             placefileRecordLock_ {};
};

PlacefileManager::PlacefileManager() : p(std::make_unique<Impl>(this)) {}
PlacefileManager::~PlacefileManager() = default;

std::vector<std::shared_ptr<gr::Placefile>>
PlacefileManager::GetActivePlacefiles()
{
   std::vector<std::shared_ptr<gr::Placefile>> placefiles;

   std::shared_lock lock {p->placefileRecordLock_};

   for (const auto& record : p->placefileRecords_)
   {
      if (record->enabled_)
      {
         placefiles.emplace_back(record->placefile_);
      }
   }

   return placefiles;
}

void PlacefileManager::LoadFile(const std::string& filename)
{
   logger_->debug("LoadFile: {}", filename);

   boost::asio::post(
      p->threadPool_,
      [=, this]()
      {
         // Load file
         std::shared_ptr<gr::Placefile> placefile =
            gr::Placefile::Load(filename);

         if (placefile == nullptr)
         {
            return;
         }

         std::unique_lock lock(p->placefileRecordLock_);

         // Determine if the placefile has been loaded previously
         auto it = std::find_if(p->placefileRecords_.begin(),
                                p->placefileRecords_.end(),
                                [&filename](auto& record)
                                { return record->name_ == filename; });
         if (it != p->placefileRecords_.end())
         {
            // If the placefile has been loaded previously, update it
            (*it)->Update(placefile);

            Q_EMIT PlacefileUpdated(filename);
         }
         else
         {
            // If this is a new placefile, add it
            auto& record = p->placefileRecords_.emplace_back(
               std::make_unique<PlacefileRecord>(filename, placefile));

            Q_EMIT PlacefileEnabled(filename, record->enabled_);
            Q_EMIT PlacefileUpdated(filename);
         }
      });
}

void PlacefileRecord::Update(std::shared_ptr<gr::Placefile> placefile)
{
   // Update placefile
   placefile_ = placefile;

   // TODO: Update refresh timer
}

std::shared_ptr<PlacefileManager> PlacefileManager::Instance()
{
   static std::weak_ptr<PlacefileManager> placefileManagerReference_ {};
   static std::mutex                      instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<PlacefileManager> placefileManager =
      placefileManagerReference_.lock();

   if (placefileManager == nullptr)
   {
      placefileManager           = std::make_shared<PlacefileManager>();
      placefileManagerReference_ = placefileManager;
   }

   return placefileManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
