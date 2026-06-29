#pragma once

#include <atomic>

namespace scwx::common
{

class ApplicationState
{
public:
   explicit ApplicationState() = delete;
   ~ApplicationState()         = delete;

   ApplicationState(const ApplicationState&)            = delete;
   ApplicationState& operator=(const ApplicationState&) = delete;

   ApplicationState(ApplicationState&&) noexcept            = delete;
   ApplicationState& operator=(ApplicationState&&) noexcept = delete;

   static std::atomic<bool>& IsRunning();
   static void               Shutdown();
};

} // namespace scwx::common
