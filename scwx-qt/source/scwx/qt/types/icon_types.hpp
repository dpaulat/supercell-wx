#pragma once

#include <scwx/qt/util/texture_atlas.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

struct IconInfo
{
   IconInfo(const std::string& iconSheet,
            std::size_t        iconWidth,
            std::size_t        iconHeight,
            std::int32_t       hotX,
            std::int32_t       hotY) :
       iconSheet_ {iconSheet},
       iconWidth_ {iconWidth},
       iconHeight_ {iconHeight},
       hotX_ {hotX},
       hotY_ {hotY}
   {
   }

   void UpdateTextureInfo();

   std::string             iconSheet_;
   std::size_t             iconWidth_;
   std::size_t             iconHeight_;
   std::int32_t            hotX_;
   std::int32_t            hotY_;
   util::TextureAttributes texture_ {};
   std::size_t             rows_ {};
   std::size_t             columns_ {};
   std::size_t             numIcons_ {};
   float                   scaledWidth_ {};
   float                   scaledHeight_ {};
};

} // namespace types
} // namespace qt
} // namespace scwx
