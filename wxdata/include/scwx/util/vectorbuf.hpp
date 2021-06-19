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

protected:
   pos_type
   seekoff(std::streamoff          off,
           std::ios_base::seekdir  way,
           std::ios_base::openmode which = std::ios_base::in |
                                           std::ios_base::out) override;

private:
   std::vector<char>& v_;
};

} // namespace util
} // namespace scwx
