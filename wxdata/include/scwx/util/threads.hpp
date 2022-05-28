#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

namespace scwx
{
namespace util
{

boost::asio::io_context& io_context();

template<class F>
void async(F&& f)
{
   boost::asio::post(io_context(), f);
}

} // namespace util
} // namespace scwx
