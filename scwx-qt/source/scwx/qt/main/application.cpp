#include <scwx/qt/main/application.hpp>
#include <scwx/util/logger.hpp>

#include <condition_variable>

namespace scwx
{
namespace qt
{
namespace main
{
namespace Application
{

static const std::string logPrefix_ = "scwx::qt::main::application";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static std::mutex              initializationMutex_ {};
static std::condition_variable initializationCondition_ {};
static bool                    initialized_ {false};

void FinishInitialization()
{
   logger_->debug("Application initialization finished");

   // Set initialized to true
   std::unique_lock lock(initializationMutex_);
   initialized_ = true;
   lock.unlock();

   // Notify any threads waiting for initialization
   initializationCondition_.notify_all();
}

void WaitForInitialization()
{
   std::unique_lock lock(initializationMutex_);

   // While not yet initialized
   while (!initialized_)
   {
      // Wait for initialization
      initializationCondition_.wait(lock);
   }
}

} // namespace Application
} // namespace main
} // namespace qt
} // namespace scwx
