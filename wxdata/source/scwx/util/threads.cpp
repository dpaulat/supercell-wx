#include <scwx/util/threads.hpp>

namespace scwx
{
namespace util
{

boost::asio::io_context& io_context()
{
   static boost::asio::io_context ioContext {};
   return ioContext;
}

} // namespace util
} // namespace scwx
