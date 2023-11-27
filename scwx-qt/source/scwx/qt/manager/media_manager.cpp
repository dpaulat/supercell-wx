#include <scwx/qt/manager/media_manager.hpp>
#include <scwx/util/logger.hpp>

#include <QAudioDevice>
#include <QAudioOutput>
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
       mediaPlayer_ {new QMediaPlayer(self)},
       audioOutput_ {new QAudioOutput(self)}
   {
      audioOutput_->setVolume(1.0f);
      mediaPlayer_->setAudioOutput(audioOutput_);

      logger_->debug("Audio device: {}",
                     audioOutput_->device().description().toStdString());

      ConnectSignals();
   }

   ~Impl() {}

   void ConnectSignals();

   MediaManager* self_;

   QMediaPlayer* mediaPlayer_;
   QAudioOutput* audioOutput_;
};

MediaManager::MediaManager() : p(std::make_unique<Impl>(this)) {}
MediaManager::~MediaManager() = default;

void MediaManager::Impl::ConnectSignals()
{
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

   logger_->debug("Playing audio: {}", path);

   p->mediaPlayer_->setSource(QUrl(QString::fromStdString(path)));

   QMetaObject::invokeMethod(p->mediaPlayer_, &QMediaPlayer::play);
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
