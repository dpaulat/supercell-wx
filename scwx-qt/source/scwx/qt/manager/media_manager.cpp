#include <scwx/qt/manager/media_manager.hpp>
#include <scwx/util/logger.hpp>

#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QUrl>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::media_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MediaManager::Impl
{
public:
   explicit Impl(MediaManager* self) :
       self_ {self},
       mediaDevices_ {new QMediaDevices(self)},
       mediaPlayer_ {new QMediaPlayer(self)},
       audioOutput_ {new QAudioOutput(self)}
   {
      logger_->debug("Audio device: {}",
                     audioOutput_->device().description().toStdString());

      mediaPlayer_->setAudioOutput(audioOutput_);

      ConnectSignals();
   }

   ~Impl() {}

   void ConnectSignals();

   MediaManager* self_;

   QMediaDevices* mediaDevices_;
   QMediaPlayer*  mediaPlayer_;
   QAudioOutput*  audioOutput_;
};

MediaManager::MediaManager() : p(std::make_unique<Impl>(this)) {}
MediaManager::~MediaManager() = default;

void MediaManager::Impl::ConnectSignals()
{
   QObject::connect(
      mediaDevices_,
      &QMediaDevices::audioOutputsChanged,
      self_,
      [this]()
      { audioOutput_->setDevice(QMediaDevices::defaultAudioOutput()); });

   QObject::connect(audioOutput_,
                    &QAudioOutput::deviceChanged,
                    self_,
                    [this]()
                    {
                       logger_->debug(
                          "Audio device changed: {}",
                          audioOutput_->device().description().toStdString());
                    });

   QObject::connect(mediaPlayer_,
                    &QMediaPlayer::errorOccurred,
                    self_,
                    [](QMediaPlayer::Error error, const QString& errorString)
                    {
                       logger_->error("Error {}: {}",
                                      static_cast<int>(error),
                                      errorString.toStdString());
                    });
}

void MediaManager::Play(types::AudioFile media)
{
   const std::string path = types::GetMediaPath(media);
}

void MediaManager::Play(const std::string& mediaPath)
{
   logger_->debug("Playing audio: {}", mediaPath);

   if (mediaPath.starts_with(':'))
   {
      p->mediaPlayer_->setSource(
         QUrl(QString("qrc%1").arg(QString::fromStdString(mediaPath))));
   }
   else
   {
      p->mediaPlayer_->setSource(
         QUrl::fromLocalFile(QString::fromStdString(mediaPath)));
   }

   p->mediaPlayer_->setPosition(0);

   QMetaObject::invokeMethod(p->mediaPlayer_, &QMediaPlayer::play);
}

void MediaManager::Stop()
{
   QMetaObject::invokeMethod(p->mediaPlayer_, &QMediaPlayer::stop);
}

std::shared_ptr<MediaManager> MediaManager::Instance()
{
   static std::weak_ptr<MediaManager> mediaManagerReference_ {};
   static std::mutex                  instanceMutex_ {};

   std::unique_lock lock(instanceMutex_);

   std::shared_ptr<MediaManager> mediaManager = mediaManagerReference_.lock();

   if (mediaManager == nullptr)
   {
      mediaManager           = std::make_shared<MediaManager>();
      mediaManagerReference_ = mediaManager;
   }

   return mediaManager;
}

} // namespace manager
} // namespace qt
} // namespace scwx
