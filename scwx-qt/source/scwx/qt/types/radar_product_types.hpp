#pragma once

#include <cstdint>

namespace scwx::qt::types
{

enum class RadarProductLoadStatus : std::uint8_t
{
   ProductNotLoaded,
   ProductLoaded,
   ListingProducts,
   LoadingProduct,
   ProductNotAvailable
};

}
