#include <scwx/qt/types/text_event_key.hpp>

#include <format>

#include <boost/container_hash/hash.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::string logPrefix_ = "scwx::qt::types::text_event_key";

std::string TextEventKey::ToString() const
{
   return std::format("{}, {}, {}, {}",
                      officeId_,
                      awips::GetPhenomenonText(phenomenon_),
                      awips::GetSignificanceText(significance_),
                      etn_);
}

bool TextEventKey::operator==(const TextEventKey& o) const
{
   return (officeId_ == o.officeId_ && phenomenon_ == o.phenomenon_ &&
           significance_ == o.significance_ && etn_ == o.etn_);
}

size_t TextEventHash<TextEventKey>::operator()(const TextEventKey& x) const
{
   size_t seed = 0;
   boost::hash_combine(seed, x.officeId_);
   boost::hash_combine(seed, x.phenomenon_);
   boost::hash_combine(seed, x.significance_);
   boost::hash_combine(seed, x.etn_);
   return seed;
}

} // namespace types
} // namespace qt
} // namespace scwx
