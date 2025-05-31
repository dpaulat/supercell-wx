#pragma once
#include <scwx/deriver/base_deriver.hpp>

namespace scwx::deriver
{

class SrvDeriver : public BaseDeriver
{
public:
   explicit SrvDeriver();
   ~SrvDeriver() override;

   SrvDeriver(const SrvDeriver&)            = delete;
   SrvDeriver(SrvDeriver&&)                 = delete;
   SrvDeriver& operator=(const SrvDeriver&) = delete;
   SrvDeriver& operator=(SrvDeriver&&)      = delete;

   bool                                   NeedsLevel2Input() override;
   const std::unordered_set<std::string>& GetLevel3InputProducts() override;
   std::shared_ptr<data::DerivedData>     GetOutput() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
