#pragma once

#include <scwx/wsr88d/ar2v_file.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace scwx
{
namespace provider
{

class Level2DataProviderImpl;

class Level2DataProvider
{
public:
   explicit Level2DataProvider();
   ~Level2DataProvider();

   Level2DataProvider(const Level2DataProvider&) = delete;
   Level2DataProvider& operator=(const Level2DataProvider&) = delete;

   Level2DataProvider(Level2DataProvider&&) noexcept;
   Level2DataProvider& operator=(Level2DataProvider&&) noexcept;

   virtual size_t cache_size() const = 0;

   /**
    * Finds the most recent key in the cache, no later than the time provided.
    *
    * @param time Upper-bound time for the key search
    *
    * @return Level 2 data key
    */
   virtual std::string FindKey(std::chrono::system_clock::time_point time) = 0;

   /**
    * Lists level 2 objects for the date supplied, and adds them to the cache.
    *
    * @param date Date for which to list objects
    *
    * @return - New objects found for the given date
    *         - Total objects found for the given date
    */
   virtual std::pair<size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date) = 0;

   /**
    * Loads a level 2 object by the given key.
    *
    * @param key Level 2 data key
    *
    * @return Level 2 data
    */
   virtual std::shared_ptr<wsr88d::Ar2vFile>
   LoadObjectByKey(const std::string& key) = 0;

   /**
    * Lists level 2 objects for the current date, and adds them to the cache. If
    * no objects have been added to the cache for the current date, the previous
    * date is also queried for data.
    *
    * @return New objects found
    */
   virtual size_t Refresh() = 0;

private:
   std::unique_ptr<Level2DataProviderImpl> p;
};

} // namespace provider
} // namespace scwx
