#include <scwx/qt/types/icon_types.hpp>

namespace scwx
{
namespace qt
{
namespace types
{

void IconInfo::SetAnchor(float anchorX, float anchorY)
{
   anchor_ = {anchorX, anchorY};
}

void IconInfo::UpdateTextureInfo()
{
   texture_ = util::TextureAtlas::Instance().GetTextureAttributes(iconSheet_);

   if (iconWidth_ > 0 && iconHeight_ > 0)
   {
      columns_ = texture_.size_.x / iconWidth_;
      rows_    = texture_.size_.y / iconHeight_;
   }
   else
   {
      columns_ = 1u;
      rows_    = 1u;

      iconWidth_  = static_cast<std::size_t>(texture_.size_.x);
      iconHeight_ = static_cast<std::size_t>(texture_.size_.y);
   }

   if (hotX_ == -1 || hotY_ == -1)
   {
      if (anchor_.has_value())
      {
         hotX_ =
            std::clamp<std::int32_t>(static_cast<std::int32_t>(std::lround(
                                        iconWidth_ * anchor_.value().first)),
                                     0,
                                     static_cast<std::int32_t>(iconWidth_));
         hotY_ =
            std::clamp<std::int32_t>(static_cast<std::int32_t>(std::lround(
                                        iconHeight_ * anchor_.value().second)),
                                     0,
                                     static_cast<std::int32_t>(iconHeight_));
      }
      else
      {
         hotX_ = static_cast<std::int32_t>(iconWidth_ / 2);
         hotY_ = static_cast<std::int32_t>(iconHeight_ / 2);
      }
   }

   numIcons_ = columns_ * rows_;

   // Pixel size
   float xFactor = 0.0f;
   float yFactor = 0.0f;

   if (texture_.size_.x > 0 && texture_.size_.y > 0)
   {
      xFactor = (texture_.sRight_ - texture_.sLeft_) / texture_.size_.x;
      yFactor = (texture_.tBottom_ - texture_.tTop_) / texture_.size_.y;
   }

   scaledWidth_  = iconWidth_ * xFactor;
   scaledHeight_ = iconHeight_ * yFactor;
}

} // namespace types
} // namespace qt
} // namespace scwx
