#pragma once

#include <cstdint>
#include <string>

namespace scwx::provider
{

struct StrikeData
{
   double  latitude {0.0};
   double  longitude {0.0};
   int64_t time_ns {0};
   int     polarity {0};
   int     delay {0};
};

} // namespace scwx::provider
