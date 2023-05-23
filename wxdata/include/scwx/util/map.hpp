#pragma once

#include <map>
#include <optional>

namespace scwx
{
namespace util
{

template<class Container, class ReturnType = Container::const_pointer>
ReturnType GetBoundedElementPointer(Container& container,
                                    const typename Container::key_type& key)
{
   ReturnType elementPtr {nullptr};

   // Find the first element greater than the key requested
   auto it = container.upper_bound(key);

   // An element with a key greater was found
   if (it != container.cend())
   {
      // Are there elements prior to this element?
      if (it != container.cbegin())
      {
         // Get the element immediately preceding, this the element we are
         // looking for
         elementPtr = &(*(--it));
      }
      else
      {
         // The current element is a good substitute
         elementPtr = &(*it);
      }
   }
   else if (container.size() > 0)
   {
      // An element with a key greater was not found. If it exists, it must be
      // the last element.
      elementPtr = &(*container.rbegin());
   }

   return elementPtr;
}

template<class Key, class T, class ReturnType = std::optional<T>>
ReturnType GetBoundedElement(std::map<Key, T>& map, const Key& key)
{
   ReturnType element;

   typename std::map<Key, T>::pointer elementPtr =
      GetBoundedElementPointer<std::map<Key, T>,
                               typename std::map<Key, T>::pointer>(map, key);
   if (elementPtr != nullptr)
   {
      element = elementPtr->second;
   }

   return element;
}

template<class Key, class T>
inline T GetBoundedElementValue(std::map<Key, T>& map, const Key& key)
{
   return GetBoundedElement<Key, T, T>(map, key);
}

} // namespace util
} // namespace scwx
