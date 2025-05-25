#pragma once
#include <scwx/derivers/base_deriver.hpp>

namespace scwx::derivers
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

   bool                                   needs_level_2_input() override;
   const std::unordered_set<std::string>& get_level_3_input_products() override;
   std::shared_ptr<data::DerivedData>     get_output() override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
