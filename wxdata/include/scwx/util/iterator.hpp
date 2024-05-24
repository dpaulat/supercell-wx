#pragma once

#include <type_traits>

namespace scwx
{
namespace util
{

template<typename T, T beginValue, T endValue>
class Iterator
{
   typedef typename std::underlying_type<T>::type value_t;
   int                                            value_;

public:
   Iterator(const T& v) : value_(static_cast<value_t>(v)) {}
   Iterator() : value_(static_cast<value_t>(beginValue)) {}
   Iterator operator++()
   {
      ++value_;
      return *this;
   }
   T        operator*() { return static_cast<T>(value_); }
   Iterator begin() { return *this; } // Default constructor
   Iterator end()
   {
      static const Iterator endIterator = ++Iterator(endValue);
      return endIterator;
   }
   std::size_t count()
   {
      return static_cast<value_t>(endValue) - static_cast<value_t>(beginValue) +
             1;
   }
   bool operator!=(const Iterator& i) { return value_ != i.value_; }
};

} // namespace util
} // namespace scwx
