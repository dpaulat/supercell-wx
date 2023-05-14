#include <scwx/qt/manager/timeline_manager.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::timeline_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class TimelineManager::Impl
{
public:
   explicit Impl(TimelineManager* self) : self_ {self} {}

   ~Impl() {}

   TimelineManager* self_;
};

TimelineManager::TimelineManager() : p(std::make_unique<Impl>(this)) {}
TimelineManager::~TimelineManager() = default;

std::shared_ptr<TimelineManager> TimelineManager::Instance()
{
   static std::weak_ptr<TimelineManager> timelineManagerReference_ {};
   static std::mutex                     instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<TimelineManager> timelineManager =
      timelineManagerReference_.lock();

   if (timelineManager == nullptr)
   {
      timelineManager           = std::make_shared<TimelineManager>();
      timelineManagerReference_ = timelineManager;
   }

   return timelineManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
