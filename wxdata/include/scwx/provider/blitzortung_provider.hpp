#pragma once

#include <scwx/provider/blitzortung_data.hpp>

#include <functional>
#include <memory>
#include <string>

namespace scwx::provider
{

class BlitzortungProvider
{
public:
   using StrikeCallback = std::function<void(const StrikeData&)>;

   explicit BlitzortungProvider();
   ~BlitzortungProvider();

   BlitzortungProvider(const BlitzortungProvider&)            = delete;
   BlitzortungProvider& operator=(const BlitzortungProvider&) = delete;
   BlitzortungProvider(BlitzortungProvider&&)                 = delete;
   BlitzortungProvider& operator=(BlitzortungProvider&&)      = delete;

   void Start();
   void Stop();
   bool IsActive() const;

   void SetStrikeCallback(StrikeCallback callback);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
