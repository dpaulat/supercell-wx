#pragma once

#include <streambuf>
#include <vector>

namespace scwx
{
namespace util
{

class rangebuf : public std::streambuf
{
public:
   rangebuf(std::streambuf* sbuf, size_t size);
   ~rangebuf() = default;

   rangebuf(const rangebuf&) = delete;
   rangebuf& operator=(const rangebuf&) = delete;

   int underflow() override;

private:
   size_t            size_;
   std::streambuf*   sbuf_;
   std::vector<char> data_;
};

} // namespace util
} // namespace scwx
