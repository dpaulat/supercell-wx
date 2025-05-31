#pragma once

#include <scwx/deriver/data/derived_data.hpp>
#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/level3_file.hpp>
#include <scwx/wsr88d/nexrad_file.hpp>
#include <scwx/common/products.hpp>

#include <memory>
#include <unordered_set>

#include <boost/signals2/signal.hpp>

namespace scwx::deriver
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
    * Gets the signal invoked when the output data is updated
    *
    * @return Changed signal
    */
   [[nodiscard]] boost::signals2::signal<void()>& update_signal() const;

   /**
    * Feed the deriver a new level 2 file. The file should not change once fed.
    *
    * @param file The level 2 file to update the deriver with
    */
   void SetLevel2InputFile(std::shared_ptr<wsr88d::Ar2vFile> file);

   /**
    * Feed the deriver a new level 3 file for a specific product. The file
    * should not change once fed.
    *
    * @param product The name of the product to feed the file
    * @param file The level 3 file to update the deriver with
    */
   void SetLevel3InputFile(const std::string&                  product,
                           std::shared_ptr<wsr88d::Level3File> file);

   /**
    * Retreve the latest derived data from the cache.
    *
    * @return The derived data the subclass has calculate
    */
   [[nodiscard]] std::shared_ptr<data::DerivedData> GetOutput();

   /**
    * If the subclass derives from level 2 products.
    *
    * @return If the subclass uses level 2 products
    */
   virtual bool NeedsLevel2Input() = 0;

   /**
    * The level 3 products that the subclass derives from.
    *
    * @return The level 3 products that the subclass derives from
    */
   virtual const std::unordered_set<std::string>& GetLevel3InputProducts() = 0;

protected:
   /**
    * Allows the subclass to get the current level 2 file.
    *
    * @return The current level 2 file
    */
   std::shared_ptr<wsr88d::Ar2vFile> GetLevel2File();

   /**
    * Allows the subclass to get the current level 3 file for a product.
    *
    * @param product The name of the product to get the file for
    *
    * @return The current level 3 file
    */
   std::shared_ptr<wsr88d::Level3File>
   GetLevel3File(const std::string& product);

   /**
    * The method where the subclass should calculate the derived product.
    *
    * @return The newly calculated data.
    */
   virtual std::shared_ptr<data::DerivedData> CalculateData() = 0;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::deriver
