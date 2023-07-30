#include <scwx/qt/util/network.hpp>

#include <QDir>
#include <QUrl>

namespace scwx
{
namespace qt
{
namespace util
{
namespace network
{

std::string NormalizeUrl(const std::string& urlString)
{
   std::string normalizedUrl;

   // Normalize URL string
   QUrl url = QUrl::fromUserInput(QString::fromStdString(urlString));
   if (url.isLocalFile())
   {
      normalizedUrl = QDir::toNativeSeparators(url.toLocalFile()).toStdString();
   }
   else
   {
      normalizedUrl = urlString;
   }

   return normalizedUrl;
}

} // namespace network
} // namespace util
} // namespace qt
} // namespace scwx
