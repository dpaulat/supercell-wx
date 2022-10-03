#pragma once

#include <string>
#include <utility>

namespace scwx
{
namespace util
{

template<class Key>
struct hash;

template<>
struct hash<std::pair<std::string, std::string>>
{
   size_t operator()(const std::pair<std::string, std::string>& x) const;
};

} // namespace util
} // namespace scwx
