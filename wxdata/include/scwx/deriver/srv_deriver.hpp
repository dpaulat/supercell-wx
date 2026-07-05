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

   std::shared_ptr<data::DerivedData>
   GetOutput(const std::string& product) override;

   static const std::unordered_map<std::string, DerivedProductInfo>&
   deriveable_products();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
