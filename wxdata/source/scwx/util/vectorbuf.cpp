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

vectorbuf::pos_type vectorbuf::seekoff(std::streamoff          off,
                                       std::ios_base::seekdir  way,
                                       std::ios_base::openmode which)
{
   // Adapted from Microsoft stringbuf reference implementation
   // Copyright (c) Microsoft Corporation.
   // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

   // Change position by off, according to way, which
   const auto gptrOld  = gptr();
   const auto pptrOld  = pptr();
   const auto seekHigh = (pptrOld == nullptr) ? egptr() : pptr();

   const auto seekLow  = eback();
   const auto seekDist = seekHigh - seekLow;
   off_type   newOffset;
   switch (way)
   {
   case std::ios_base::beg: newOffset = 0; break;
   case std::ios_base::end: newOffset = seekDist; break;
   case std::ios_base::cur:
   {
      constexpr auto BOTH = std::ios_base::in | std::ios_base::out;
      if ((which & BOTH) != BOTH)
      {
         if (which & std::ios_base::in)
         {
            if (gptrOld || !seekLow)
            {
               newOffset = gptrOld - seekLow;
               break;
            }
         }
         else if ((which & std::ios_base::out) && (pptrOld || !seekLow))
         {
            newOffset = pptrOld - seekLow;
            break;
         }
      }
   }

   default: return pos_type(off_type(-1));
   }

   if (static_cast<unsigned long long>(off) + newOffset >
       static_cast<unsigned long long>(seekDist))
   {
      return pos_type(off_type(-1));
   }

   off += newOffset;
   if (off != 0 && (((which & std::ios_base::in) && !gptrOld) ||
                    ((which & std::ios_base::out) && !pptrOld)))
   {
      return pos_type(off_type(-1));
   }

   const auto next = seekLow + off;
   if ((which & std::ios_base::in) && gptrOld)
   {
      setg(seekLow, next, seekHigh);
   }

   if ((which & std::ios_base::out) && pptrOld)
   {
      setp(seekLow, next, epptr());
   }

   return pos_type(off);
}

} // namespace util
} // namespace scwx
