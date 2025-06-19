#pragma once

namespace scwx::deriver::data
{
class DerivedData
{
public:
   explicit DerivedData()                     = default;
   virtual ~DerivedData()                     = default;
   DerivedData(const DerivedData&)            = delete;
   DerivedData(DerivedData&&)                 = default;
   DerivedData& operator=(const DerivedData&) = delete;
   DerivedData& operator=(DerivedData&&)      = default;
};
} // namespace scwx::deriver::data
