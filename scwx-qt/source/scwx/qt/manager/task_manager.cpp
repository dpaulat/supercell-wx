#include <scwx/qt/manager/task_manager.hpp>
#include <scwx/qt/manager/radar_site_status_manager.hpp>
#include <scwx/network/ntp_client.hpp>
#include <scwx/util/logger.hpp>

namespace scwx::qt::manager::TaskManager
{

static const std::string logPrefix_ = "scwx::qt::manager::task_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static std::shared_ptr<network::NtpClient> ntpClient_ {};
static std::shared_ptr<manager::RadarSiteStatusManager>
   radarSiteStatusManager_ {};

void Initialize()
{
   logger_->debug("Initialize");

   ntpClient_              = network::NtpClient::Instance();
   radarSiteStatusManager_ = manager::RadarSiteStatusManager::Instance();

   ntpClient_->Start();
   ntpClient_->WaitForInitialOffset();

   radarSiteStatusManager_->Start();
}

void Shutdown()
{
   logger_->debug("Shutdown");

   radarSiteStatusManager_->Stop();
   ntpClient_->Stop();

   radarSiteStatusManager_.reset();
   ntpClient_.reset();
}

} // namespace scwx::qt::manager::TaskManager
