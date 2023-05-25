#pragma once

#include <scwx/wsr88d/nexrad_file.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace provider
{

class NexradDataProvider
{
public:
   explicit NexradDataProvider();
   virtual ~NexradDataProvider();

   NexradDataProvider(const NexradDataProvider&)            = delete;
   NexradDataProvider& operator=(const NexradDataProvider&) = delete;

   NexradDataProvider(NexradDataProvider&&) noexcept;
   NexradDataProvider& operator=(NexradDataProvider&&) noexcept;

   virtual size_t cache_size() const = 0;

   /**
    * Gets the last modified time. This is equal to the most recent object's
    * modification time. If there are no objects, the epoch is returned.
    *
    * @return Last modified time
    */
   virtual std::chrono::system_clock::time_point last_modified() const = 0;

   /**
    * Gets the current update period. This is equal to the difference between
    * the last two objects' modification times. If there are less than two
    * objects, an update period of 0 is returned.
    *
    * @return Update period
    */
   virtual std::chrono::seconds update_period() const = 0;

   /**
    * Finds the most recent key in the cache, no later than the time provided.
    *
    * @param time Upper-bound time for the key search
    *
    * @return NEXRAD data key
    */
   virtual std::string FindKey(std::chrono::system_clock::time_point time) = 0;

   /**
    * Finds the most recent key in the cache.
    *
    * @return NEXRAD data key
    */
   virtual std::string FindLatestKey() = 0;

   /**
    * Lists NEXRAD objects for the date supplied, and adds them to the cache.
    *
    * @param date Date for which to list objects
    *
    * @return - Whether query was successful
    *         - New objects found for the given date
    *         - Total objects found for the given date
    */
   virtual std::tuple<bool, size_t, size_t>
   ListObjects(std::chrono::system_clock::time_point date) = 0;

   /**
    * Loads a NEXRAD file object by the given key.
    *
    * @param key NEXRAD data key
    *
    * @return NEXRAD data
    */
   virtual std::shared_ptr<wsr88d::NexradFile>
   LoadObjectByKey(const std::string& key) = 0;

   /**
    * Lists NEXRAD objects for the current date, and adds them to the cache. If
    * no objects have been added to the cache for the current date, the previous
    * date is also queried for data.
    *
    * @return - New objects found
    *         - Total objects found
    */

   virtual std::pair<size_t, size_t> Refresh() = 0;

   /**
    * Convert the object key to a time point.
    *
    * @param key NEXRAD data key
    *
    * @return NEXRAD data time point
    */
   virtual std::chrono::system_clock::time_point
   GetTimePointByKey(const std::string& key) const = 0;

   /**
    * Gets NEXRAD data time points for the date supplied. Lists and adds them
    * to the cache if required.
    *
    * @param date Date for which to get NEXRAD data time points
    *
    * @return NEXRAD data time points
    */
   virtual std::vector<std::chrono::system_clock::time_point>
   GetTimePointsByDate(std::chrono::system_clock::time_point date) = 0;

   /**
    * Requests available NEXRAD products for the current radar site, and adds
    * the list to the cache.
    */
   virtual void RequestAvailableProducts();

   /**
    * Gets the list of available NEXRAD products for the current radar site.
    *
    * @return Available NEXRAD products
    */
   virtual std::vector<std::string> GetAvailableProducts();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace provider
} // namespace scwx
