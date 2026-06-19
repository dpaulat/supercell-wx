#include <scwx/qt/types/placefile_types.hpp>

#include <QDir>
#include <QUrl>

namespace scwx::qt::types
{

PlacefileImageInfo::PlacefileImageInfo(const std::string& imageFile,
                                       const std::string& baseUrlString)
{
   // Resolve using base URL
   const auto baseUrl =
      QUrl::fromUserInput(QString::fromStdString(baseUrlString));
   const auto relativeUrl =
      QUrl(QDir::fromNativeSeparators(QString::fromStdString(imageFile)));
   resolvedUrl_ = baseUrl.resolved(relativeUrl).toString().toStdString();
}

void PlacefileImageInfo::UpdateTextureInfo()
{
   texture_ = util::TextureAtlas::Instance().GetTextureAttributes(resolvedUrl_);

   scaledWidth_  = texture_.sRight_ - texture_.sLeft_;
   scaledHeight_ = texture_.tBottom_ - texture_.tTop_;
}

} // namespace scwx::qt::types
