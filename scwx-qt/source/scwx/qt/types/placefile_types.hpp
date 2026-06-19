#pragma once

#include <string>

#include <scwx/qt/util/texture_atlas.hpp>

namespace scwx::qt::types
{

struct PlacefileImageInfo
{
   PlacefileImageInfo(const std::string& imageFile,
                      const std::string& baseUrlString);

   void UpdateTextureInfo();

   std::string             resolvedUrl_ {};
   util::TextureAttributes texture_ {};
   float                   scaledWidth_ {};
   float                   scaledHeight_ {};
};

} // namespace scwx::qt::types
