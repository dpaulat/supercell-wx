#include <scwx/util/hash.hpp>

#include <boost/container_hash/hash.hpp>

namespace scwx
{
namespace util
{

size_t hash<std::pair<std::string, std::string>>::operator()(
   const std::pair<std::string, std::string>& x) const
{
   size_t seed = 0;
   boost::hash_combine(seed, x.first);
   boost::hash_combine(seed, x.second);
   return seed;
}

} // namespace util
} // namespace scwx
