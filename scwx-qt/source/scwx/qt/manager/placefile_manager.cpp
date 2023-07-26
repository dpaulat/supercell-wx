#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/gr/placefile.hpp>
#include <scwx/util/logger.hpp>

#include <shared_mutex>
#include <vector>

#include <QDir>
#include <QUrl>
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
   bool                           thresholded_ {true};
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

   std::vector<std::shared_ptr<PlacefileRecord>> placefileRecords_ {};
   std::unordered_map<std::string, std::shared_ptr<PlacefileRecord>>
                     placefileRecordMap_ {};
   std::shared_mutex placefileRecordLock_ {};
};

PlacefileManager::PlacefileManager() : p(std::make_unique<Impl>(this)) {}
PlacefileManager::~PlacefileManager() = default;

bool PlacefileManager::placefile_enabled(const std::string& name)
{
   std::shared_lock lock(p->placefileRecordLock_);

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      return it->second->enabled_;
   }
   return false;
}

bool PlacefileManager::placefile_thresholded(const std::string& name)
{
   std::shared_lock lock(p->placefileRecordLock_);

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      return it->second->thresholded_;
   }
   return false;
}

std::shared_ptr<const gr::Placefile>
PlacefileManager::placefile(const std::string& name)
{
   std::shared_lock lock(p->placefileRecordLock_);

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      return it->second->placefile_;
   }
   return nullptr;
}

void PlacefileManager::set_placefile_enabled(const std::string& name,
                                             bool               enabled)
{
   std::shared_lock lock(p->placefileRecordLock_);

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      it->second->enabled_ = enabled;

      lock.unlock();

      Q_EMIT PlacefileEnabled(name, enabled);
   }
}

void PlacefileManager::set_placefile_thresholded(const std::string& name,
                                                 bool               thresholded)
{
   std::shared_lock lock(p->placefileRecordLock_);

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      it->second->thresholded_ = thresholded;

      lock.unlock();

      Q_EMIT PlacefileUpdated(name);
   }
}

void PlacefileManager::set_placefile_url(const std::string& name,
                                         const std::string& newUrl)
{
   std::unique_lock lock(p->placefileRecordLock_);

   auto it    = p->placefileRecordMap_.find(name);
   auto itNew = p->placefileRecordMap_.find(newUrl);
   if (it != p->placefileRecordMap_.cend() &&
       itNew == p->placefileRecordMap_.cend())
   {
      auto placefileRecord        = it->second;
      placefileRecord->name_      = newUrl;
      placefileRecord->placefile_ = nullptr;
      p->placefileRecordMap_.erase(it);
      p->placefileRecordMap_.emplace(newUrl, placefileRecord);

      lock.unlock();

      Q_EMIT PlacefileRenamed(name, newUrl);
   }
}

std::vector<std::shared_ptr<gr::Placefile>>
PlacefileManager::GetActivePlacefiles()
{
   std::vector<std::shared_ptr<gr::Placefile>> placefiles;

   std::shared_lock lock {p->placefileRecordLock_};

   for (const auto& record : p->placefileRecords_)
   {
      if (record->enabled_ && record->placefile_ != nullptr)
      {
         placefiles.emplace_back(record->placefile_);
      }
   }

   return placefiles;
}

void PlacefileManager::AddUrl(const std::string& urlString)
{
   std::string normalizedUrl;

   // Normalize URL string
   QUrl url = QUrl::fromUserInput(QString::fromStdString(urlString));
   if (url.isLocalFile())
   {
      normalizedUrl = QDir::toNativeSeparators(url.toLocalFile()).toStdString();
   }
   else
   {
      normalizedUrl = urlString;
   }

   std::unique_lock lock(p->placefileRecordLock_);

   // Determine if the placefile has been loaded previously
   auto it = std::find_if(p->placefileRecords_.begin(),
                          p->placefileRecords_.end(),
                          [&normalizedUrl](auto& record)
                          { return record->name_ == normalizedUrl; });
   if (it != p->placefileRecords_.end())
   {
      logger_->debug("Placefile already added: {}", normalizedUrl);
      return;
   }

   // Placefile is new, proceed with adding
   logger_->info("AddUrl: {}", normalizedUrl);

   // Add an empty placefile record for the new URL
   auto placefileRecord = p->placefileRecords_.emplace_back(
      std::make_shared<PlacefileRecord>(normalizedUrl, nullptr, false));
   p->placefileRecordMap_.insert_or_assign(normalizedUrl, placefileRecord);

   lock.unlock();

   Q_EMIT PlacefileUpdated(normalizedUrl);
}

void PlacefileManager::LoadFile(const std::string& filename)
{
   const std::string placefileName =
      QDir::toNativeSeparators(QString::fromStdString(filename)).toStdString();

   logger_->debug("LoadFile: {}", placefileName);

   boost::asio::post(
      p->threadPool_,
      [placefileName, this]()
      {
         // Load file
         std::shared_ptr<gr::Placefile> placefile =
            gr::Placefile::Load(placefileName);

         if (placefile == nullptr)
         {
            return;
         }

         std::unique_lock lock(p->placefileRecordLock_);

         // Determine if the placefile has been loaded previously
         auto it = p->placefileRecordMap_.find(placefileName);
         if (it != p->placefileRecordMap_.end())
         {
            // If the placefile has been loaded previously, update it
            it->second->Update(placefile);

            lock.unlock();

            Q_EMIT PlacefileUpdated(placefileName);
         }
         else
         {
            // If this is a new placefile, add it
            auto& record = p->placefileRecords_.emplace_back(
               std::make_shared<PlacefileRecord>(placefileName, placefile));
            p->placefileRecordMap_.insert_or_assign(placefileName, record);

            lock.unlock();

            Q_EMIT PlacefileEnabled(placefileName, record->enabled_);
            Q_EMIT PlacefileUpdated(placefileName);
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
