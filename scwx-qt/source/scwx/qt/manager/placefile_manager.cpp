#include <scwx/qt/manager/placefile_manager.hpp>
#include <scwx/qt/manager/font_manager.hpp>
#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/main/application.hpp>
#include <scwx/qt/util/json.hpp>
#include <scwx/qt/util/network.hpp>
#include <scwx/gr/placefile.hpp>
#include <scwx/network/cpr.hpp>
#include <scwx/util/logger.hpp>

#include <shared_mutex>
#include <vector>

#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>
#include <boost/algorithm/string.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/json.hpp>
#include <boost/tokenizer.hpp>
#include <cpr/cpr.h>
#include <fmt/chrono.h>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::placefile_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kEnabledName_     = "enabled";
static const std::string kThresholdedName_ = "thresholded";
static const std::string kTitleName_       = "title";
static const std::string kNameName_        = "name";

class PlacefileManager::Impl
{
public:
   class PlacefileRecord;

   explicit Impl(PlacefileManager* self) : self_ {self} {}
   ~Impl() { threadPool_.join(); }

   void InitializePlacefileSettings();
   void ReadPlacefileSettings();
   void WritePlacefileSettings();

   static boost::unordered_flat_map<std::size_t,
                                    std::shared_ptr<types::ImGuiFont>>
   LoadFontResources(const std::shared_ptr<gr::Placefile>& placefile);
   static std::vector<std::shared_ptr<boost::gil::rgba8_image_t>>
   LoadImageResources(const std::shared_ptr<gr::Placefile>& placefile);

   boost::asio::thread_pool threadPool_ {1u};

   PlacefileManager* self_;

   std::string placefileSettingsPath_ {};

   std::shared_ptr<config::RadarSite> radarSite_ {};

   std::vector<std::shared_ptr<PlacefileRecord>> placefileRecords_ {};
   boost::unordered_flat_map<std::string, std::shared_ptr<PlacefileRecord>>
                     placefileRecordMap_ {};
   std::shared_mutex placefileRecordLock_ {};
};

class PlacefileManager::Impl::PlacefileRecord
{
public:
   explicit PlacefileRecord(Impl*                          impl,
                            const std::string&             name,
                            std::shared_ptr<gr::Placefile> placefile,
                            const std::string&             title   = {},
                            bool                           enabled = false,
                            bool thresholded                       = false) :
       p {impl},
       name_ {name},
       title_ {title},
       placefile_ {placefile},
       enabled_ {enabled},
       thresholded_ {thresholded}
   {
   }
   ~PlacefileRecord()
   {
      std::unique_lock refreshLock(refreshMutex_);
      std::unique_lock timerLock(timerMutex_);
      refreshTimer_.cancel();
      timerLock.unlock();
      refreshLock.unlock();

      threadPool_.join();
   }

   bool                 refresh_enabled() const;
   std::chrono::seconds refresh_time() const;

   void CancelRefresh();
   void ScheduleRefresh();
   void ScheduleRefresh(
      const std::chrono::system_clock::duration timeUntilNextUpdate);
   void Update();
   void UpdateAsync();

   friend void tag_invoke(boost::json::value_from_tag,
                          boost::json::value&                     jv,
                          const std::shared_ptr<PlacefileRecord>& record)
   {
      jv = {{kEnabledName_, record->enabled_},
            {kThresholdedName_, record->thresholded_},
            {kTitleName_, record->title_},
            {kNameName_, record->name_}};
   }

   friend PlacefileRecord tag_invoke(boost::json::value_to_tag<PlacefileRecord>,
                                     const boost::json::value& jv)
   {
      return PlacefileRecord {
         nullptr,
         boost::json::value_to<std::string>(jv.at(kNameName_)),
         nullptr,
         boost::json::value_to<std::string>(jv.at(kTitleName_)),
         jv.at(kEnabledName_).as_bool(),
         jv.at(kThresholdedName_).as_bool()};
   }

   Impl* p;

   std::string                    name_;
   std::string                    title_;
   std::shared_ptr<gr::Placefile> placefile_;
   bool                           enabled_;
   bool                           thresholded_;
   boost::asio::thread_pool       threadPool_ {1u};
   boost::asio::steady_timer      refreshTimer_ {threadPool_};
   std::mutex                     refreshMutex_ {};
   std::mutex                     timerMutex_ {};

   boost::unordered_flat_map<std::size_t, std::shared_ptr<types::ImGuiFont>>
              fonts_ {};
   std::mutex fontsMutex_ {};

   std::vector<std::shared_ptr<boost::gil::rgba8_image_t>> images_ {};

   std::string                           lastRadarSite_ {};
   std::chrono::system_clock::time_point lastUpdateTime_ {};

   std::size_t failureCount_ {};
};

PlacefileManager::PlacefileManager() : p(std::make_unique<Impl>(this))
{
   boost::asio::post(p->threadPool_,
                     [this]()
                     {
                        try
                        {
                           p->InitializePlacefileSettings();

                           // Read placefile settings on startup
                           main::Application::WaitForInitialization();
                           p->ReadPlacefileSettings();
                           Q_EMIT PlacefilesInitialized();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
}

PlacefileManager::~PlacefileManager()
{
   // Write placefile settings on shutdown
   p->WritePlacefileSettings();
};

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

std::string PlacefileManager::placefile_title(const std::string& name)
{
   std::shared_lock lock(p->placefileRecordLock_);

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      return it->second->title_;
   }
   return {};
}

std::shared_ptr<gr::Placefile>
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

boost::unordered_flat_map<std::size_t, std::shared_ptr<types::ImGuiFont>>
PlacefileManager::placefile_fonts(const std::string& name)
{
   std::shared_lock lock(p->placefileRecordLock_);

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      std::unique_lock fontsLock {it->second->fontsMutex_};
      return it->second->fonts_;
   }
   return {};
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

      using namespace std::chrono_literals;

      // Update the placefile
      if (enabled)
      {
         if (p->radarSite_ != nullptr &&
             record->lastRadarSite_ != p->radarSite_->id())
         {
            // If the radar site has changed, update now
            record->UpdateAsync();
         }
         else
         {
            // Otherwise, schedule an update
            record->ScheduleRefresh();
         }
      }
      else if (!enabled)
      {
         record->CancelRefresh();
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
   std::string normalizedUrl = util::network::NormalizeUrl(newUrl);

   std::unique_lock lock(p->placefileRecordLock_);

   auto it    = p->placefileRecordMap_.find(name);
   auto itNew = p->placefileRecordMap_.find(normalizedUrl);
   if (it != p->placefileRecordMap_.cend() &&
       itNew == p->placefileRecordMap_.cend())
   {
      auto placefileRecord        = it->second;
      placefileRecord->name_      = normalizedUrl;
      placefileRecord->placefile_ = nullptr;
      placefileRecord->fonts_.clear();
      placefileRecord->images_.clear();
      p->placefileRecordMap_.erase(it);
      p->placefileRecordMap_.insert_or_assign(normalizedUrl, placefileRecord);

      lock.unlock();

      Q_EMIT PlacefileRenamed(name, normalizedUrl);

      // Queue a placefile update
      placefileRecord->UpdateAsync();
   }
}

bool PlacefileManager::Impl::PlacefileRecord::refresh_enabled() const
{
   if (placefile_ != nullptr)
   {
      using namespace std::chrono_literals;
      return placefile_->refresh() > 0s;
   }

   return false;
}

std::chrono::seconds
PlacefileManager::Impl::PlacefileRecord::refresh_time() const
{
   using namespace std::chrono_literals;

   if (refresh_enabled())
   {
      // Don't refresh more often than every 1 second
      return std::max(placefile_->refresh(), 1s);
   }

   return -1s;
}

void PlacefileManager::Impl::InitializePlacefileSettings()
{
   std::string appDataPath {
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         .toStdString()};

   if (!std::filesystem::exists(appDataPath))
   {
      if (!std::filesystem::create_directories(appDataPath))
      {
         logger_->error("Unable to create application data directory: \"{}\"",
                        appDataPath);
      }
   }

   placefileSettingsPath_ = appDataPath + "/placefiles.json";
}

void PlacefileManager::Impl::ReadPlacefileSettings()
{
   logger_->info("Reading placefile settings");

   boost::json::value placefileJson = nullptr;

   // Determine if placefile settings exists
   if (std::filesystem::exists(placefileSettingsPath_))
   {
      placefileJson = util::json::ReadJsonFile(placefileSettingsPath_);
   }

   // If placefile settings was successfully read
   if (placefileJson != nullptr && placefileJson.is_array())
   {
      // For each placefile entry
      auto& placefileArray = placefileJson.as_array();
      for (auto& placefileEntry : placefileArray)
      {
         try
         {
            // Convert placefile entry to a record
            PlacefileRecord record =
               boost::json::value_to<PlacefileRecord>(placefileEntry);

            if (!record.name_.empty())
            {
               self_->AddUrl(record.name_,
                             record.title_,
                             record.enabled_,
                             record.thresholded_);
            }
         }
         catch (const std::exception& ex)
         {
            logger_->warn("Invalid placefile entry: {}", ex.what());
         }
      }
   }
}

void PlacefileManager::Impl::WritePlacefileSettings()
{
   logger_->info("Saving placefile settings");

   std::shared_lock lock {placefileRecordLock_};
   auto             placefileJson = boost::json::value_from(placefileRecords_);
   util::json::WriteJsonFile(placefileSettingsPath_, placefileJson);
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

void PlacefileManager::AddUrl(const std::string& urlString,
                              const std::string& title,
                              bool               enabled,
                              bool               thresholded)
{
   std::string normalizedUrl = util::network::NormalizeUrl(urlString);

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
         p.get(), normalizedUrl, nullptr, title, enabled, thresholded));
   p->placefileRecordMap_.insert_or_assign(normalizedUrl, record);

   lock.unlock();

   if (enabled)
   {
      Q_EMIT PlacefileEnabled(normalizedUrl, record->enabled_);
   }

   Q_EMIT PlacefileUpdated(normalizedUrl);

   // Queue a placefile update, either if enabled, or if we don't know the title
   if (enabled || title.empty())
   {
      record->UpdateAsync();
   }
}

void PlacefileManager::RemoveUrl(const std::string& urlString)
{
   std::unique_lock lock(p->placefileRecordLock_);

   // Determine if the placefile has been loaded previously
   auto it = std::find_if(p->placefileRecords_.begin(),
                          p->placefileRecords_.end(),
                          [&urlString](auto& record)
                          { return record->name_ == urlString; });
   if (it == p->placefileRecords_.end())
   {
      logger_->debug("Placefile doesn't exist: {}", urlString);
      return;
   }

   // Placefile exists, proceed with removing
   logger_->info("RemoveUrl: {}", urlString);

   // Remove record
   p->placefileRecords_.erase(it);
   p->placefileRecordMap_.erase(urlString);

   lock.unlock();

   Q_EMIT PlacefileRemoved(urlString);
}

void PlacefileManager::Refresh(const std::string& name)
{
   std::shared_lock lock {p->placefileRecordLock_};

   auto it = p->placefileRecordMap_.find(name);
   if (it != p->placefileRecordMap_.cend())
   {
      it->second->UpdateAsync();
   }
}

void PlacefileManager::Impl::PlacefileRecord::Update()
{
   logger_->debug("Update: {}", name_);

   // Take unique lock before refreshing
   std::unique_lock lock {refreshMutex_};

   // Make a copy of name in the event it changes.
   const std::string name {name_};

   std::shared_ptr<gr::Placefile> updatedPlacefile {};

   QUrl url = QUrl::fromUserInput(QString::fromStdString(name));
   if (url.isLocalFile())
   {
      updatedPlacefile = gr::Placefile::Load(name);

      if (updatedPlacefile == nullptr)
      {
         logger_->error("Local placefile not found: {}", name);
      }
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
         {"version", "1.5"}, // Placefile Version Supported
         {"dpi", fmt::format("{:0.0f}", dpi)},
         {"lat", fmt::format("{:0.3f}", p->radarSite_->latitude())},
         {"lon", fmt::format("{:0.3f}", p->radarSite_->longitude())}};

      // Iterate through each query parameter in the URL
      if (url.hasQuery())
      {
         auto query = url.query(QUrl::ComponentFormattingOption::PrettyDecoded)
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
      // Load placefile resources
      auto newFonts  = Impl::LoadFontResources(updatedPlacefile);
      auto newImages = Impl::LoadImageResources(updatedPlacefile);

      // Check the name matches, in case the name updated
      if (name_ == name)
      {
         // Update the placefile
         placefile_      = updatedPlacefile;
         title_          = placefile_->title();
         lastUpdateTime_ = std::chrono::system_clock::now();
         failureCount_   = 0;

         // Update font resources
         {
            std::unique_lock fontsLock {fontsMutex_};
            fonts_.swap(newFonts);
            newFonts.clear();
         }

         // Update image resources
         images_.swap(newImages);
         newImages.clear();

         if (p->radarSite_ != nullptr)
         {
            lastRadarSite_ = p->radarSite_->id();
         }

         // Notify slots of the placefile update
         Q_EMIT p->self_->PlacefileUpdated(name);
      }

      // Update refresh timer
      ScheduleRefresh();
   }
   else if (enabled_)
   {
      using namespace std::chrono_literals;

      ++failureCount_;

      // Update refresh timer if the file failed to load, in case it is able to
      // be resolved later
      if (url.isLocalFile())
      {
         ScheduleRefresh(10s);
      }
      else
      {
         // Start attempting to refresh at 15 seconds, and start backing off
         // until retrying every 60 seconds
         ScheduleRefresh(
            std::min<std::chrono::seconds>(15s * failureCount_, 60s));
      }
   }
}

void PlacefileManager::Impl::PlacefileRecord::ScheduleRefresh()
{
   using namespace std::chrono_literals;

   if (!enabled_ || !refresh_enabled())
   {
      // Refresh is disabled
      return;
   }

   std::unique_lock lock {timerMutex_};

   auto nextUpdateTime      = lastUpdateTime_ + refresh_time();
   auto timeUntilNextUpdate = nextUpdateTime - std::chrono::system_clock::now();

   ScheduleRefresh(timeUntilNextUpdate);
}

void PlacefileManager::Impl::PlacefileRecord::ScheduleRefresh(
   const std::chrono::system_clock::duration timeUntilNextUpdate)
{
   logger_->debug(
      "Scheduled refresh in {:%M:%S} ({})",
      std::chrono::duration_cast<std::chrono::seconds>(timeUntilNextUpdate),
      name_);

   refreshTimer_.expires_after(timeUntilNextUpdate);
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
            UpdateAsync();
         }
      });
}

void PlacefileManager::Impl::PlacefileRecord::CancelRefresh()
{
   std::unique_lock lock {timerMutex_};
   refreshTimer_.cancel();
}

void PlacefileManager::Impl::PlacefileRecord::UpdateAsync()
{
   boost::asio::post(threadPool_,
                     [this]()
                     {
                        try
                        {
                           Update();
                        }
                        catch (const std::exception& ex)
                        {
                           logger_->error(ex.what());
                        }
                     });
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

boost::unordered_flat_map<std::size_t, std::shared_ptr<types::ImGuiFont>>
PlacefileManager::Impl::LoadFontResources(
   const std::shared_ptr<gr::Placefile>& placefile)
{
   boost::unordered_flat_map<std::size_t, std::shared_ptr<types::ImGuiFont>>
        imGuiFonts {};
   auto fonts = placefile->fonts();

   for (auto& font : fonts)
   {
      units::font_size::pixels<double> size {font.second->pixels_};
      std::vector<std::string>         styles {};

      if (font.second->IsBold())
      {
         styles.push_back("bold");
      }
      if (font.second->IsItalic())
      {
         styles.push_back("italic");
      }

      auto imGuiFont = FontManager::Instance().LoadImGuiFont(
         font.second->face_, styles, size);
      imGuiFonts.emplace(font.first, std::move(imGuiFont));
   }

   return imGuiFonts;
}

std::vector<std::shared_ptr<boost::gil::rgba8_image_t>>
PlacefileManager::Impl::LoadImageResources(
   const std::shared_ptr<gr::Placefile>& placefile)
{
   const auto iconFiles = placefile->icon_files();
   const auto drawItems = placefile->GetDrawItems();

   const QUrl baseUrl =
      QUrl::fromUserInput(QString::fromStdString(placefile->name()));

   std::vector<std::string> urlStrings {};
   urlStrings.reserve(iconFiles.size());

   // Resolve Icon Files
   std::transform(iconFiles.cbegin(),
                  iconFiles.cend(),
                  std::back_inserter(urlStrings),
                  [&baseUrl](auto& iconFile)
                  {
                     // Resolve target URL relative to base URL
                     QString filePath =
                        QString::fromStdString(iconFile->filename_);
                     QUrl fileUrl = QUrl(QDir::fromNativeSeparators(filePath));
                     QUrl resolvedUrl = baseUrl.resolved(fileUrl);

                     return resolvedUrl.toString().toStdString();
                  });

   // Resolve Image Files
   for (auto& di : drawItems)
   {
      switch (di->itemType_)
      {
      case gr::Placefile::ItemType::Image:
      {
         const std::string& imageFile =
            std::static_pointer_cast<gr::Placefile::ImageDrawItem>(di)
               ->imageFile_;

         QString     filePath    = QString::fromStdString(imageFile);
         QUrl        fileUrl     = QUrl(QDir::fromNativeSeparators(filePath));
         QUrl        resolvedUrl = baseUrl.resolved(fileUrl);
         std::string urlString   = resolvedUrl.toString().toStdString();

         if (std::find(urlStrings.cbegin(), urlStrings.cend(), urlString) ==
             urlStrings.cend())
         {
            urlStrings.push_back(urlString);
         }
         break;
      }

      default:
         break;
      }
   }

   return ResourceManager::LoadImageResources(urlStrings);
}

} // namespace manager
} // namespace qt
} // namespace scwx
