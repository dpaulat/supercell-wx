#include <scwx/provider/nws_level3_behavior.hpp>

#include <atomic>

namespace scwx::provider
{

class NwsLevel3Behavior::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)                = delete;
   Impl& operator=(const Impl&)     = delete;
   Impl(Impl&&) noexcept            = delete;
   Impl& operator=(Impl&&) noexcept = delete;

   std::atomic<bool> running_ {true};
};

NwsLevel3Behavior::NwsLevel3Behavior() :
    IHttpLevel3ServerBehavior(), p {std::make_unique<Impl>()}
{
}

NwsLevel3Behavior::~NwsLevel3Behavior() = default;

void NwsLevel3Behavior::Shutdown() noexcept
{
   p->running_ = false;
}

std::vector<std::string>
NwsLevel3Behavior::ListObjects(std::chrono::system_clock::time_point date)
{
   // TODO: Implement
   (void) date;
   return {};
}

std::string NwsLevel3Behavior::GetFileUrl(const std::string& key) const
{
   // TODO: Implement
   (void) key;
   return {};
}

std::chrono::system_clock::time_point
NwsLevel3Behavior::GetTimePointByKey(const std::string& key) const
{
   // TODO: Implement
   (void) key;
   return {};
}

void NwsLevel3Behavior::RequestAvailableProducts()
{
   // TODO: Implement
}

std::vector<std::string> NwsLevel3Behavior::GetAvailableProducts() const
{
   // TODO: Implement
   return {};
}

bool NwsLevel3Behavior::date_archive_available() const
{
   // Not supported
   return false;
}

} // namespace scwx::provider
