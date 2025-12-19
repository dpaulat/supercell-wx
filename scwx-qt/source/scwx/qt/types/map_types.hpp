#pragma once

#include <string>

namespace scwx::qt::types
{

static constexpr std::size_t kMapCount_ = 9u;

enum class AnimationState
{
   Play,
   Pause
};

enum class MapTime
{
   Live,
   Archive
};

enum class NoUpdateReason
{
   NoChange,
   NotLoaded,
   NotAvailable,
   InvalidProduct,
   InvalidData
};

std::string GetMapTimeName(MapTime mapTime);

} // namespace scwx::qt::types
