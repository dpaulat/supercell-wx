#include <scwx/qt/manager/alert_manager.hpp>
#include <scwx/qt/manager/position_manager.hpp>
#include <scwx/qt/settings/audio_settings.hpp>
#include <scwx/qt/types/location_types.hpp>
#include <scwx/util/logger.hpp>

#include <boost/uuid/random_generator.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::alert_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class AlertManager::Impl
{
public:
   explicit Impl(AlertManager* self) : self_ {self}
   {
      settings::AudioSettings& audioSettings =
         settings::AudioSettings::Instance();

      UpdateLocationTracking(audioSettings.alert_location_method().GetValue());

      audioSettings.alert_location_method().RegisterValueChangedCallback(
         [this](const std::string& value) { UpdateLocationTracking(value); });
   }

   ~Impl() {}

   void UpdateLocationTracking(const std::string& value) const;

   AlertManager* self_;

   boost::uuids::uuid uuid_ {boost::uuids::random_generator()()};

   std::shared_ptr<PositionManager> positionManager_ {
      PositionManager::Instance()};
};

AlertManager::AlertManager() : p(std::make_unique<Impl>(this)) {}
AlertManager::~AlertManager() = default;

void AlertManager::Impl::UpdateLocationTracking(
   const std::string& locationMethodName) const
{
   types::LocationMethod locationMethod =
      types::GetLocationMethod(locationMethodName);
   bool locationEnabled = locationMethod == types::LocationMethod::Track;
   positionManager_->EnablePositionUpdates(uuid_, locationEnabled);
}

std::shared_ptr<AlertManager> AlertManager::Instance()
{
   static std::weak_ptr<AlertManager> alertManagerReference_ {};
   static std::mutex                  instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<AlertManager> alertManager = alertManagerReference_.lock();

   if (alertManager == nullptr)
   {
      alertManager           = std::make_shared<AlertManager>();
      alertManagerReference_ = alertManager;
   }

   return alertManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
