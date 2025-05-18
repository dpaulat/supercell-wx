#pragma once
#include <scwx/derivers/base_deriver.hpp>

namespace scwx::deriver
{

class SrvDeriver
{
public:
   explicit SrvDeriver();
   ~SrvDeriver();

   SrvDeriver(const SrvDeriver&)            = delete;
   SrvDeriver(SrvDeriver&&)                 = delete;
   SrvDeriver& operator=(const SrvDeriver&) = delete;
   SrvDeriver& operator=(SrvDeriver&&)      = delete;

   bool                                   needs_level_2_input();
   const std::unordered_set<std::string>& get_level_3_input_products();
   std::shared_ptr<wsr88d::NexradFile>    get_output_file();
   void set_level_2_input_file(std::shared_ptr<wsr88d::NexradFile> file);
   void set_level_3_input_file(const std::string&                  product,
                               std::shared_ptr<wsr88d::NexradFile> file);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
