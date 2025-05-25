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


   /**
    * Feed the deriver a new level 2 file.
    *
    * @param file The level 2 file to update the deriver with
    */
   void set_level_2_input_file(std::shared_ptr<wsr88d::Ar2vFile> file);

   /**
    * Feed the deriver a new level 3 file for a specific product.
    *
    * @param product The name of the product to feed the file
    * @param file The level 3 file to update the deriver with
    */
   void set_level_3_input_file(const std::string&                  product,
                               std::shared_ptr<wsr88d::Level3File> file);

   /**
    * If the subclass derives from level 2 products.
    *
    * @return If the subclass uses level 2 products
    */
   virtual bool needs_level_2_input() = 0;

   /**
    * The level 3 products that the subclass derives from.
    *
    * @return The level 3 products that the subclass derives from
    */
   virtual const std::unordered_set<std::string>&
                                              get_level_3_input_products() = 0;

   /**
    * Retreve the latest derived data from the subclass. Data may be updated if
    * Inputs have been updated.
    *
    * @return The derived data the subclass has calculate
    */
   virtual std::shared_ptr<data::DerivedData> get_output()                 = 0;

protected:
   /**
    * Allows the subclass to get the current level 2 file.
    *
    * @return The current level 2 file
    */
   std::shared_ptr<wsr88d::Ar2vFile> get_level_2_file();

   /**
    * Allows the subclass to get the current level 3 file for a product.
    *
    * @param product The name of the product to get the file for
    *
    * @return The current level 3 file
    */
   std::shared_ptr<wsr88d::Level3File>
   get_level_3_file(const std::string& product);

   /**
    * Gets if any of the files have been changed since the last time data was
    * retrieve.
    *
    * @return If any of the files have been changed
    */
   bool get_changed();

   /**
    * Sets the changed flag. Normally used to clear it by the subclass.
    *
    * @param changed The new value of the changed flag
    */
   void set_changed(bool changed);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
