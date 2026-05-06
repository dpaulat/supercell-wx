#include <scwx/qt/ui/api_key_edit_widget.hpp>
#include <scwx/util/logger.hpp>

#include <QMetaEnum>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QApplication>
#include <QStyle>
#include <QUrlQuery>
#include <QToolTip>

using namespace scwx::qt::ui;

static const std::string logPrefix_ = "scwx::qt::ui::QApiKeyEdit";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

QApiKeyEdit::QApiKeyEdit(QWidget* parent) :
    QLineEdit(parent), networkAccessManager_(new QNetworkAccessManager(this))
{
   const QIcon icon =
      QApplication::style()->standardIcon(QStyle::SP_BrowserReload);
   testAction_ = addAction(icon, QLineEdit::TrailingPosition);
   testAction_->setIconText(tr("Test Key"));
   testAction_->setToolTip(tr("Test the API key for this provider"));

   connect(testAction_, &QAction::triggered, this, &QApiKeyEdit::apiTest);
   connect(networkAccessManager_,
           &QNetworkAccessManager::finished,
           this,
           &QApiKeyEdit::apiTestFinished);

   // Reset test icon when text changes
   connect(this,
           &QLineEdit::textChanged,
           this,
           [this, icon]() { testAction_->setIcon(icon); });
}

void QApiKeyEdit::apiTest()
{
   QNetworkRequest req;
   req.setTransferTimeout(5000);

   switch (provider_)
   {
   case map::MapProvider::Mapbox:
   {
      QUrl url("https://api.mapbox.com/v4/mapbox.mapbox-streets-v8/1/0/0.mvt");
      logger_->debug("Testing MapProvider::Mapbox API key at {}",
                     url.toString().toStdString());
      QUrlQuery query;
      query.addQueryItem("access_token", text());
      url.setQuery(query);
      req.setUrl(url);
      break;
   }
   case map::MapProvider::MapTiler:
   {
      QUrl url("https://api.maptiler.com/maps/streets-v2/");
      logger_->debug("Testing MapProvider::MapTiler API key at {}",
                     url.toString().toStdString());
      QUrlQuery query;
      query.addQueryItem("key", text());
      url.setQuery(query);
      req.setUrl(url);
      break;
   }
   case map::MapProvider::OpenFreeMap:
      // No API key for this OpenFreeMap, so just return.
      return;
   default:
   {
      logger_->warn("Cannot test MapProvider::Unknown API key");
      break;
   }
   }

   networkAccessManager_->get(req);
}

void QApiKeyEdit::apiTestFinished(QNetworkReply* reply)
{
   switch (reply->error())
   {
   case QNetworkReply::NoError:
   {
      logger_->info("QApiKeyEdit: test success");
      QToolTip::showText(mapToGlobal(QPoint()), tr("Key was valid"));
      testAction_->setIcon(
         QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton));
      Q_EMIT apiTestSucceeded();
      break;
   }
   default:
   {
      const char* errStr =
         QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(
            reply->error());
      logger_->warn("QApiKeyEdit: test failed, got {} from {}",
                    errStr,
                    reply->url().host().toStdString());
      QToolTip::showText(mapToGlobal(QPoint()), tr("Invalid key"));
      testAction_->setIcon(
         QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton));
      Q_EMIT apiTestFailed(reply->error());
      break;
   }
   }
}
