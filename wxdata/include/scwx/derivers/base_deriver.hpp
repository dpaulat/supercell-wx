#pragma once

#include <scwx/wsr88d/nexrad_file.hpp>
#include <scwx/common/products.hpp>

#include <memory>
#include <unordered_set>

namespace scwx::deriver
{

class BaseDeriver
{
public:
   virtual ~BaseDeriver();

   BaseDeriver(const BaseDeriver&)            = default;
   BaseDeriver(BaseDeriver&&)                 = delete;
   BaseDeriver& operator=(const BaseDeriver&) = default;
   BaseDeriver& operator=(BaseDeriver&&)      = delete;

   virtual bool needs_level_2_input() = 0;
   virtual const std::unordered_set<std::string>&
                                               get_level_3_input_products() = 0;
   virtual std::shared_ptr<wsr88d::NexradFile> get_output_file()            = 0;
   virtual void
   set_level_2_input_file(std::shared_ptr<wsr88d::NexradFile> file) = 0;
   virtual void
   set_level_3_input_file(const std::string&                  product,
                          std::shared_ptr<wsr88d::NexradFile> file) = 0;
};

} // namespace scwx::deriver
