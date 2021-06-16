#include <scwx/util/vectorbuf.hpp>

namespace scwx
{
namespace util
{

vectorbuf::vectorbuf(std::vector<char>& v) : v_(v)
{
   update_read_pointers(0);
}

void vectorbuf::update_read_pointers(size_t size)
{
   setg(v_.data(), v_.data(), v_.data() + size);
}

} // namespace util
} // namespace scwx
