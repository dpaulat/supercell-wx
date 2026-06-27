#include <scwx/common/application_state.hpp>

#include <atomic>

namespace scwx::common
{

static std::atomic<bool> running_ {true};

bool ApplicationState::IsRunning()
{
   return running_;
}

void ApplicationState::Shutdown()
{
   running_ = false;
}

} // namespace scwx::common
