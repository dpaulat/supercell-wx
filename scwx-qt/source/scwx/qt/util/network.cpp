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
   QString trimmedUrlString = QString::fromStdString(urlString).trimmed();
   QUrl url = QUrl::fromUserInput(trimmedUrlString);
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
