#include <scwx/qt/manager/media_manager.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::media_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MediaManager::Impl
{
public:
   explicit Impl() {}

   ~Impl() {}
};

MediaManager::MediaManager() : p(std::make_unique<Impl>()) {}
MediaManager::~MediaManager() = default;

std::shared_ptr<MediaManager> MediaManager::Instance()
{
   static std::weak_ptr<MediaManager> mediaManagerReference_ {};
   static std::mutex                  instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<MediaManager> mediaManager = mediaManagerReference_.lock();

   if (mediaManager == nullptr)
   {
      mediaManager           = std::make_shared<MediaManager>();
      mediaManagerReference_ = mediaManager;
   }

   return mediaManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
