#include <scwx/qt/manager/update_manager.hpp>
#include <scwx/util/logger.hpp>

#include <mutex>

#include <boost/json.hpp>
#include <cpr/cpr.h>
#include <re2/re2.h>
#include <QStandardPaths>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::update_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static const std::string kGithubApiBase {"https://api.github.com"};
static const std::string kScwxReleaseEndpoint {
   kGithubApiBase + "/repos/dpaulat/supercell-wx/releases"};

class UpdateManager::Impl
{
public:
   explicit Impl(UpdateManager* self) : self_ {self} {}

   ~Impl() {}

   static std::string        GetVersionString(const std::string& releaseName);
   static boost::json::value ParseResponseText(const std::string& s);

   size_t PopulateReleases();
   size_t AddReleases(const boost::json::value& json);
   std::pair<std::vector<types::gh::Release>::iterator, std::string>
   FindLatestRelease();

   UpdateManager* self_;

   std::mutex updateMutex_ {};

   std::vector<types::gh::Release> releases_ {};
   types::gh::Release              latestRelease_ {};
   std::string                     latestVersion_ {};
};

UpdateManager::UpdateManager() : p(std::make_unique<Impl>(this)) {}
UpdateManager::~UpdateManager() = default;

types::gh::Release UpdateManager::latest_release() const
{
   return p->latestRelease_;
}

std::string UpdateManager::latest_version() const
{
   return p->latestVersion_;
}

std::string
UpdateManager::Impl::GetVersionString(const std::string& releaseName)
{
   static constexpr LazyRE2 re = {"(\\d+\\.\\d+\\.\\d+)"};
   std::string              versionString {};

   RE2::PartialMatch(releaseName, *re, &versionString);

   return versionString;
}

boost::json::value UpdateManager::Impl::ParseResponseText(const std::string& s)
{
   boost::json::stream_parser p;
   boost::json::error_code    ec;

   p.write(s, ec);
   if (ec)
   {
      logger_->warn("{}", ec.message());
      return nullptr;
   }

   p.finish(ec);
   if (ec)
   {
      logger_->warn("{}", ec.message());
      return nullptr;
   }

   return p.release();
}

bool UpdateManager::CheckForUpdates(const std::string& currentVersion)
{
   std::unique_lock lock(p->updateMutex_);

   logger_->info("Checking for updates");

   // Query GitHub for releases
   size_t numReleases = p->PopulateReleases();
   bool   newRelease  = false;

   // If GitHub returned valid releases
   if (numReleases > 0)
   {
      // Get the latest release
      auto [latestRelease, latestVersion] = p->FindLatestRelease();

      // Validate the latest release, and compare to the current version
      if (latestRelease != p->releases_.end() && latestVersion > currentVersion)
      {
         logger_->info("An update is available: {}", latestVersion);

         p->latestRelease_ = *latestRelease;
         p->latestVersion_ = latestVersion;
         newRelease        = true;
         Q_EMIT UpdateAvailable(latestVersion, *latestRelease);
      }
   }

   return newRelease;
}

size_t UpdateManager::Impl::PopulateReleases()
{
   static constexpr size_t perPage =
      100u;                // The number of results per page (max 100)
   size_t page       = 1u; // Page number of the results to fetch
   size_t numResults = 0u;

   static const std::string perPageString {fmt::format("{}", perPage)};

   // Clear any existing releases
   releases_.clear();

   do
   {
      const std::string pageString {fmt::format("{}", page)};

      cpr::Response r = cpr::Get(
         cpr::Url {kScwxReleaseEndpoint},
         cpr::Parameters {{"per_page", perPageString}, {"page", pageString}},
         cpr::Header {{"accept", "application/vnd.github+json"},
                      {"X-GitHub-Api-Version", "2022-11-28"}});

      // Successful REST API query
      if (r.status_code == 200)
      {
         boost::json::value json = Impl::ParseResponseText(r.text);
         if (json == nullptr)
         {
            logger_->warn("Response not JSON: {}", r.header["content-type"]);
            break;
         }

         // Add results from response
         size_t newResults = AddReleases(json);
         numResults += newResults;

         if (newResults < perPage)
         {
            // We have reached the last page of results
            break;
         }
      }
      else
      {
         logger_->warn(
            "Invalid API response: [{}] {}", r.status_code, r.error.message);
         break;
      }

      // Check page is less than 100, this is to prevent an infinite loop
   } while (++page < 100);

   return numResults;
}

size_t UpdateManager::Impl::AddReleases(const boost::json::value& json)
{
   // Parse releases
   std::vector<types::gh::Release> newReleases {};
   try
   {
      newReleases =
         boost::json::value_to<std::vector<types::gh::Release>>(json);
   }
   catch (const std::exception& ex)
   {
      logger_->warn("Error parsing JSON: {}", ex.what());
   }

   size_t newReleaseCount = newReleases.size();

   // Add releases to the current list
   releases_.insert(releases_.end(), newReleases.begin(), newReleases.end());

   return newReleaseCount;
}

std::pair<std::vector<types::gh::Release>::iterator, std::string>
UpdateManager::Impl::FindLatestRelease()
{
   // Initialize the latest release to the end iterator
   std::vector<types::gh::Release>::iterator latestRelease = releases_.end();
   std::string                               latestReleaseVersion {};

   for (auto it = releases_.begin(); it != releases_.end(); ++it)
   {
      if (it->draft_ || it->prerelease_)
      {
         // Skip drafts and prereleases
         continue;
      }

      // Get the version string of the current release
      std::string currentVersion {GetVersionString(it->name_)};

      // If not set, or current version is lexographically newer
      if (latestRelease == releases_.end() ||
          currentVersion > latestReleaseVersion)
      {
         // Update the latest release
         latestRelease        = it;
         latestReleaseVersion = currentVersion;
      }
   }

   return {latestRelease, latestReleaseVersion};
}

void UpdateManager::RemoveTemporaryReleases()
{
#if defined(_WIN32)
   const std::string destination {
      QStandardPaths::writableLocation(QStandardPaths::TempLocation)
         .toStdString()};
   const std::filesystem::path         destinationPath {destination};
   std::filesystem::directory_iterator it {destinationPath};

   for (auto& file : it)
   {
      if (file.is_regular_file() && file.path().string().ends_with(".msi") &&
          file.path().stem().string().starts_with("supercell-wx-"))
      {
         logger_->info("Removing temporary installer: {}",
                       file.path().string());

         std::error_code error;
         if (!std::filesystem::remove(file.path(), error))
         {
            logger_->warn("Error removing temporary installer: {}",
                          error.message());
         }
      }
   }
#endif
}

std::shared_ptr<UpdateManager> UpdateManager::Instance()
{
   static std::weak_ptr<UpdateManager> updateManagerReference_ {};
   static std::mutex                   instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<UpdateManager> updateManager =
      updateManagerReference_.lock();

   if (updateManager == nullptr)
   {
      updateManager           = std::make_shared<UpdateManager>();
      updateManagerReference_ = updateManager;
   }

   return updateManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
