#pragma once

#include <map>
#include <optional>

namespace scwx
{
namespace util
{

template<class Key, class T, class ReturnType = std::optional<T>>
ReturnType GetBoundedElement(std::map<Key, T>& map, Key key)
{
   ReturnType element;

   // Find the first element greater than the key requested
   auto it = map.upper_bound(key);

   // An element with a key greater was found
   if (it != map.cend())
   {
      // Are there elements prior to this element?
      if (it != map.cbegin())
      {
         // Get the element immediately preceding, this the element we are
         // looking for
         element = (--it)->second;
      }
      else
      {
         // The current element is a good substitute
         element = it->second;
      }
   }
   else if (map.size() > 0)
   {
      // An element with a key greater was not found. If it exists, it must be
      // the last element.
      element = map.rbegin()->second;
   }

   return element;
}

template<class Key, class T>
inline T GetBoundedElementValue(std::map<Key, T>& map, Key key)
{
   return GetBoundedElement<Key, T, T>(map, key);
}

} // namespace util
} // namespace scwx
