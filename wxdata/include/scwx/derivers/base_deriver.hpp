#pragma once

#include <scwx/derivers/data/derived_data.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/nexrad_file.hpp>
#include <scwx/common/products.hpp>

#include <memory>
#include <unordered_set>

namespace scwx::derivers
{

class BaseDeriver
{
public:
   explicit BaseDeriver();
   virtual ~BaseDeriver();

   BaseDeriver(const BaseDeriver&)            = delete;
   BaseDeriver(BaseDeriver&&)                 = delete;
   BaseDeriver& operator=(const BaseDeriver&) = delete;
   BaseDeriver& operator=(BaseDeriver&&)      = delete;

   virtual bool needs_level_2_input() = 0;
   virtual const std::unordered_set<std::string>&
                                              get_level_3_input_products() = 0;
   virtual std::shared_ptr<data::DerivedData> get_output()                 = 0;
   void set_level_2_input_file(std::shared_ptr<wsr88d::Ar2vFile> file);
   void set_level_3_input_file(const std::string&                  product,
                               std::shared_ptr<wsr88d::Level3File> file);

protected:
   std::shared_ptr<wsr88d::Ar2vFile> get_level_2_file();
   std::shared_ptr<wsr88d::Level3File>
   get_level_3_file(const std::string& product);
   bool get_changed();
   void set_changed(bool changed);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
