#include <future>

namespace scwx
{
namespace util
{

template<class F>
void async(F&& f)
{
   auto future = std::make_shared<std::future<void>>();
   *future     = std::async(std::launch::async, [future, f]() { f(); });
}

} // namespace util
} // namespace scwx
