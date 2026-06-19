#pragma once

#include <algorithm>

#include <boost/algorithm/string.hpp>

#define SCWX_GET_ENUM(Type, FunctionName, nameMap)                             \
   Type FunctionName(const std::string& name)                                  \
   {                                                                           \
      auto result = std::ranges::find_if(                                      \
         nameMap,                                                              \
         [&](const std::pair<Type, std::string>& pair) -> bool                 \
         { return boost::iequals(pair.second, name); });                       \
                                                                               \
      if (result != nameMap.cend())                                            \
      {                                                                        \
         return result->first;                                                 \
      }                                                                        \
      else                                                                     \
      {                                                                        \
         return Type::Unknown;                                                 \
      }                                                                        \
   }
