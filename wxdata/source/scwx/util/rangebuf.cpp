#include <scwx/util/rangebuf.hpp>

namespace scwx
{
namespace util
{

rangebuf::rangebuf(std::streambuf* sbuf, size_t size) :
    size_(size), sbuf_(sbuf), data_()
{
   size_t bufferSize = std::max<size_t>(size, 4096u);
   data_.reserve(bufferSize);
}

int rangebuf::underflow()
{
   char*     buf     = data_.data();
   ptrdiff_t bufSize = data_.capacity();

   // Read from underlying stream buffer into own buffer until needed characters
   // have been consumed
   size_t r(sbuf_->sgetn(buf, std::min<size_t>(bufSize, size_)));
   size_ -= r;
   setg(buf, buf, buf + r);
   return gptr() == egptr() ? traits_type::eof() :
                              traits_type::to_int_type(*gptr());
}

} // namespace util
} // namespace scwx
