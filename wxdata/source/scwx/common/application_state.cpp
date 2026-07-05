#include <scwx/common/application_state.hpp>

namespace scwx::common
{

static std::atomic<bool> running_ {true};

const std::atomic<bool>& ApplicationState::IsRunning()
{
   return running_;
}

void ApplicationState::Shutdown()
{
   running_ = false;
}

} // namespace scwx::common
