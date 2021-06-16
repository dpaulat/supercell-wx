#pragma once

#include <streambuf>
#include <vector>

namespace scwx
{
namespace util
{

class vectorbuf : public std::streambuf
{
public:
   vectorbuf(std::vector<char>& v);
   ~vectorbuf() = default;

   vectorbuf(const vectorbuf&) = delete;
   vectorbuf& operator=(const vectorbuf&) = delete;

   void update_read_pointers(size_t size);

private:
   std::vector<char>& v_;
};

} // namespace util
} // namespace scwx
