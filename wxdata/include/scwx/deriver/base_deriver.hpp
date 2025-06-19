#pragma once

#include <scwx/deriver/data/derived_data.hpp>
#include <scwx/wsr88d/rda/generic_radar_data.hpp>
#include <scwx/wsr88d/rpg/level3_message.hpp>
#include <scwx/wsr88d/nexrad_file.hpp>
#include <scwx/common/products.hpp>

#include <memory>

#include <utility>

namespace scwx::deriver
{

class DerivedProductInfo
{
public:
   DerivedProductInfo(
         std::string name,
         std::vector<std::string> level3AwipsIds,
         std::vector<std::tuple<wsr88d::rda::DataBlockType, float>> level2Products) :
      name_{std::move(name)},
      level3AwipsIds_{std::move(level3AwipsIds)},
      level2Products_{std::move(level2Products)}
   {}

   std::string name_;

   std::vector<std::string>                                   level3AwipsIds_;
   std::vector<std::tuple<wsr88d::rda::DataBlockType, float>> level2Products_;
};

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
    * Feed the deriver a new level 2 file. The file should not change once fed.
    *
    * TODO
    * @param file The level 2 file to update the deriver with
    */
   void SetLevel2Input(wsr88d::rda::DataBlockType dataBlockType,
                       float                      elevation,
                       std::shared_ptr<wsr88d::rda::ElevationScan> data);

   /**
    * Feed the deriver a new level 3 file for a specific product. The file
    * should not change once fed.
    *
    * TODO
    * @param product The name of the product to feed the file
    * @param file The level 3 file to update the deriver with
    */
   void SetLevel3Input(const std::string&                          product,
                       std::shared_ptr<wsr88d::rpg::Level3Message> data);

   /**
    * The method where the subclass should calculate the derived product.
    *
    * TODO
    * @return The newly calculated data.
    */
   virtual std::shared_ptr<data::DerivedData>
   GetOutput(const std::string& product) = 0;

protected:
   /**
    * Allows the subclass to get the current level 2 file.
    * TODO
    *
    * @return The current level 2 file
    */
   std::shared_ptr<wsr88d::rda::ElevationScan>
   GetLevel2Input(wsr88d::rda::DataBlockType dataBlockType, float elevation);

   /**
    * Allows the subclass to get the current level 3 file for a product.
    * TODO
    *
    * @param product The name of the product to get the file for
    *
    * @return The current level 3 file
    */
   std::shared_ptr<wsr88d::rpg::Level3Message>
   GetLevel3Input(const std::string& product);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
