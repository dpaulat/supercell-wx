#pragma once

#include <scwx/awips/pvtec.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

struct TextEventKey
{
   TextEventKey() : TextEventKey(awips::PVtec {}) {}
   TextEventKey(const awips::PVtec& pvtec) :
       officeId_ {pvtec.office_id()},
       phenomenon_ {pvtec.phenomenon()},
       significance_ {pvtec.significance()},
       etn_ {pvtec.event_tracking_number()}
   {
   }

   std::string ToFullString() const;
   std::string ToString() const;
   bool        operator==(const TextEventKey& o) const;

   std::string         officeId_;
   awips::Phenomenon   phenomenon_;
   awips::Significance significance_;
   int16_t             etn_;
};

template<class Key>
struct TextEventHash;

template<>
struct TextEventHash<TextEventKey>
{
   size_t operator()(const TextEventKey& x) const;
};

} // namespace types
} // namespace qt
} // namespace scwx
