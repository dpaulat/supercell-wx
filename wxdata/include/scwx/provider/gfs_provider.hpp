#pragma once

#include <scwx/sounding/sounding_data.hpp>

#include <memory>
#include <optional>
#include <string>

namespace scwx::provider
{

class GfsProvider
{
public:
   explicit GfsProvider();
   ~GfsProvider();

   GfsProvider(const GfsProvider&)            = delete;
   GfsProvider& operator=(const GfsProvider&) = delete;

   GfsProvider(GfsProvider&&) noexcept;
   GfsProvider& operator=(GfsProvider&&) noexcept;

   std::optional<std::shared_ptr<sounding::SoundingData>>
   FetchSounding(double lat, double lon, int cycle, int fhr);

   void Shutdown();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::provider
