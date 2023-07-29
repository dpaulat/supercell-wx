#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/gr/placefile.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <shared_mutex>
#include <vector>

#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QUrl>
#include <boost/algorithm/string.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/tokenizer.hpp>
#include <cpr/cpr.h>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::placefile_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class PlacefileManager::Impl
{
public:
   class PlacefileRecord;

   explicit Impl(PlacefileManager* self) : self_ {self} {}
   ~Impl() {}

   static std::string NormalizeUrl(const std::string& urlString);

   boost::asio::thread_pool threadPool_ {1u};

   PlacefileManager* self_;

   std::shared_ptr<config::RadarSite> radarSite_ {};

   std::vector<std::shared_ptr<PlacefileRecord>> placefileRecords_ {};
   std::unordered_map<std::string, std::shared_ptr<PlacefileRecord>>
                     placefileRecordMap_ {};
   std::shared_mutex placefileRecordLock_ {};
};

class PlacefileManager::Impl::PlacefileRecord
{
public:
   explicit PlacefileRecord(Impl*                          impl,
                            const std::string&             name,
                            std::shared_ptr<gr::Placefile> placefile,
                            bool                           enabled = true) :
       p {impl}, name_ {name}, placefile_ {placefile}, enabled_ {enabled}
   {
   }
   ~PlacefileRecord()
   {
      std::unique_lock lock(refreshMutex_);
      refreshTimer_.cancel();
   }

   void Update();
   void UpdateAsync();
   void UpdatePlacefile(std::shared_ptr<gr::Placefile> placefile);

   Impl* p;

   std::string                    name_;
   std::shared_ptr<gr::Placefile> placefile_;
   bool                           enabled_;
   bool                           thresholded_ {true};
   boost::asio::thread_pool       threadPool_ {1u};
   boost::asio::steady_timer      refreshTimer_ {threadPool_};
   std::mutex                     refreshMutex_ {};

signals:
   void Updated(const std::string&             name,
                std::shared_ptr<gr::Placefile> placefile);
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
      auto record      = it->second;
      record->enabled_ = enabled;

      lock.unlock();

      Q_EMIT PlacefileEnabled(name, enabled);

      // Update the placefile
      // TODO: Only update if it's out of date, or if the radar site has changed
      if (enabled)
      {
         it->second->UpdateAsync();
      }
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
   std::string normalizedUrl = Impl::NormalizeUrl(newUrl);

   std::unique_lock lock(p->placefileRecordLock_);

   auto it    = p->placefileRecordMap_.find(name);
   auto itNew = p->placefileRecordMap_.find(normalizedUrl);
   if (it != p->placefileRecordMap_.cend() &&
       itNew == p->placefileRecordMap_.cend())
   {
      auto placefileRecord        = it->second;
      placefileRecord->name_      = normalizedUrl;
      placefileRecord->placefile_ = nullptr;
      p->placefileRecordMap_.erase(it);
      p->placefileRecordMap_.emplace(normalizedUrl, placefileRecord);

      lock.unlock();

      Q_EMIT PlacefileRenamed(name, normalizedUrl);

      // Queue a placefile update
      placefileRecord->UpdateAsync();
   }
}

void PlacefileManager::SetRadarSite(
   std::shared_ptr<config::RadarSite> radarSite)
{
   if (p->radarSite_ == radarSite || radarSite == nullptr)
   {
      // No action needed
      return;
   }

   logger_->debug("SetRadarSite: {}", radarSite->id());

   p->radarSite_ = radarSite;

   // Update all enabled records
   std::shared_lock lock(p->placefileRecordLock_);
   for (auto& record : p->placefileRecords_)
   {
      if (record->enabled_)
      {
         record->UpdateAsync();
      }
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
   std::string normalizedUrl = Impl::NormalizeUrl(urlString);

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
   auto& record =
      p->placefileRecords_.emplace_back(std::make_shared<Impl::PlacefileRecord>(
         p.get(), normalizedUrl, nullptr, false));
   p->placefileRecordMap_.insert_or_assign(normalizedUrl, record);

   lock.unlock();

   Q_EMIT PlacefileUpdated(normalizedUrl);

   // Queue a placefile update
   record->UpdateAsync();
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
            it->second->UpdatePlacefile(placefile);

            lock.unlock();

            Q_EMIT PlacefileUpdated(placefileName);
         }
         else
         {
            // If this is a new placefile, add it
            auto& record = p->placefileRecords_.emplace_back(
               std::make_shared<Impl::PlacefileRecord>(
                  p.get(), placefileName, placefile));
            p->placefileRecordMap_.insert_or_assign(placefileName, record);

            lock.unlock();

            Q_EMIT PlacefileEnabled(placefileName, record->enabled_);
            Q_EMIT PlacefileUpdated(placefileName);
         }
      });
}

void PlacefileManager::Impl::PlacefileRecord::Update()
{
   // Make a copy of name in the event it changes.
   const std::string name {name_};

   std::shared_ptr<gr::Placefile> updatedPlacefile {};

   QUrl url = QUrl::fromUserInput(QString::fromStdString(name));
   if (url.isLocalFile())
   {
      updatedPlacefile = gr::Placefile::Load(name);
   }
   else
   {
      std::string decodedUrl {name};
      auto        queryPos = decodedUrl.find('?');
      if (queryPos != std::string::npos)
      {
         decodedUrl.erase(queryPos);
      }

      if (p->radarSite_ == nullptr)
      {
         // Wait to process until a radar site is selected
         return;
      }

      auto dpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();

      // Specify parameters
      auto parameters = cpr::Parameters {
         {"version", "1.2"}, // Placefile Version Supported
         {"dpi", fmt::format("{:0.0f}", dpi)},
         {"lat", fmt::format("{:0.3f}", p->radarSite_->latitude())},
         {"lon", fmt::format("{:0.3f}", p->radarSite_->longitude())}};

      // Iterate through each query parameter in the URL
      if (url.hasQuery())
      {
         auto query = url.query(QUrl::ComponentFormattingOption::FullyEncoded)
                         .toStdString();

         boost::char_separator<char> delimiter("&");
         boost::tokenizer            tokens(query, delimiter);

         for (auto& token : tokens)
         {
            std::vector<std::string> split {};
            boost::split(split, token, boost::is_any_of("="));
            if (split.size() >= 2)
            {
               // Token is a key=value parameter
               parameters.Add({split[0], split[1]});
            }
            else
            {
               // Token is a single key with no value
               parameters.Add({token, {}});
            }
         }
      }

      // Send HTTP GET request
      auto response =
         cpr::Get(cpr::Url {decodedUrl}, network::cpr::GetHeader(), parameters);

      if (cpr::status::is_success(response.status_code))
      {
         std::istringstream responseBody {response.text};
         updatedPlacefile = gr::Placefile::Load(name, responseBody);
      }
      else if (response.status_code == 0)
      {
         logger_->error("Error loading placefile: {}", response.error.message);
      }
      else
      {
         logger_->error("Error loading placefile: {}", response.status_line);
      }
   }

   if (updatedPlacefile != nullptr)
   {
      // Check the name matches, in case the name updated
      if (name_ == name)
      {
         // Update the placefile
         placefile_ = updatedPlacefile;

         // Notify slots of the placefile update
         Q_EMIT p->self_->PlacefileUpdated(name);
      }
   }

   // TODO: Update refresh timer
   // TODO: Can running this function out of sync with an existing refresh timer
   // cause issues?
}

void PlacefileManager::Impl::PlacefileRecord::UpdateAsync()
{
   boost::asio::post(threadPool_, [this]() { Update(); });
}

void PlacefileManager::Impl::PlacefileRecord::UpdatePlacefile(
   std::shared_ptr<gr::Placefile> placefile)
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

std::string PlacefileManager::Impl::NormalizeUrl(const std::string& urlString)
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

   return normalizedUrl;
}

} // namespace manager
} // namespace qt
} // namespace scwx
