#include <scwx/qt/manager/media_manager.hpp>
#include <scwx/qt/settings/audio_settings.hpp>
#include <scwx/util/logger.hpp>

#include <vector>

#include <boost/signals2/connection.hpp>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QThread>
#include <QUrl>

namespace scwx::qt::manager
{

static const std::string logPrefix_ = "scwx::qt::manager::media_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class MediaManager::Impl
{
public:
   explicit Impl()
   {
      mediaParent_ = std::make_unique<QObject>();
      mediaParent_->moveToThread(&thread_);

      thread_.start();

      QMetaObject::invokeMethod(
         mediaParent_.get(),
         [this]()
         {
            // QObjects are managed by the parent
            // NOLINTBEGIN(cppcoreguidelines-owning-memory)

            logger_->debug("Creating QMediaDevices");
            mediaDevices_ = new QMediaDevices(mediaParent_.get());
            logger_->debug("Creating QMediaPlayer");
            mediaPlayer_ = new QMediaPlayer(mediaParent_.get());
            logger_->debug("Creating QAudioOutput");
            audioOutput_ = new QAudioOutput(mediaParent_.get());

            // NOLINTEND(cppcoreguidelines-owning-memory)

            logger_->debug("Audio device: {}",
                           audioOutput_->device().description().toStdString());

            mediaPlayer_->setAudioOutput(audioOutput_);

            ConnectSignals();

            SetVolume(
               settings::AudioSettings::Instance().master_volume().GetValue());
         });
   }

   ~Impl()
   {
      // Delete the media parent
      mediaParent_.reset();

      thread_.quit();
      thread_.wait();
   }

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   void ConnectSignals();
   void SetVolume(std::int64_t volume);

   QThread thread_ {};

   std::vector<boost::signals2::scoped_connection> connections_ {};

   std::unique_ptr<QObject> mediaParent_ {nullptr};
   QMediaDevices*           mediaDevices_ {nullptr};
   QMediaPlayer*            mediaPlayer_ {nullptr};
   QAudioOutput*            audioOutput_ {nullptr};
};

MediaManager::MediaManager() : p(std::make_unique<Impl>()) {}
MediaManager::~MediaManager() = default;

MediaManager::MediaManager(MediaManager&&) noexcept            = default;
MediaManager& MediaManager::operator=(MediaManager&&) noexcept = default;

void MediaManager::Impl::ConnectSignals()
{
   QObject::connect(
      mediaDevices_,
      &QMediaDevices::audioOutputsChanged,
      mediaParent_.get(),
      [this]()
      { audioOutput_->setDevice(QMediaDevices::defaultAudioOutput()); });

   QObject::connect(audioOutput_,
                    &QAudioOutput::deviceChanged,
                    mediaParent_.get(),
                    [this]()
                    {
                       logger_->debug(
                          "Audio device changed: {}",
                          audioOutput_->device().description().toStdString());
                    });

   QObject::connect(mediaPlayer_,
                    &QMediaPlayer::errorOccurred,
                    mediaParent_.get(),
                    [](QMediaPlayer::Error error, const QString& errorString)
                    {
                       logger_->error("Error {}: {}",
                                      static_cast<int>(error),
                                      errorString.toStdString());
                    });

   settings::AudioSettings& audioSettings = settings::AudioSettings::Instance();

   connections_.emplace_back(
      audioSettings.master_volume().changed_signal().connect(
         [this](const auto& event)
         {
            QMetaObject::invokeMethod(mediaParent_.get(),
                                      [this, event]()
                                      { SetVolume(event.newValue_); });
         }));
}

void MediaManager::Impl::SetVolume(std::int64_t volume)
{
   if (audioOutput_ != nullptr)
   {
      const auto linearVolume =
         QtAudio::convertVolume(static_cast<float>(volume) / 100.0f,
                                QtAudio::VolumeScale::LogarithmicVolumeScale,
                                QtAudio::VolumeScale::LinearVolumeScale);
      audioOutput_->setVolume(linearVolume);
   }
}

void MediaManager::Play(types::AudioFile media)
{
   const std::string path = types::GetMediaPath(media);
   Play(path);
}

void MediaManager::Play(const std::string& mediaPath)
{
   logger_->debug("Playing audio: {}", mediaPath);

   if (p->mediaPlayer_ == nullptr)
   {
      logger_->warn("Media player is not yet initialized");
      return;
   }

   if (mediaPath.starts_with(':'))
   {
      QMetaObject::invokeMethod(
         p->mediaPlayer_,
         &QMediaPlayer::setSource,
         QUrl(QString("qrc%1").arg(QString::fromStdString(mediaPath))));
   }
   else
   {
      QMetaObject::invokeMethod(
         p->mediaPlayer_,
         &QMediaPlayer::setSource,
         QUrl::fromLocalFile(QString::fromStdString(mediaPath)));
   }

   QMetaObject::invokeMethod(p->mediaPlayer_, &QMediaPlayer::setPosition, 0);
   QMetaObject::invokeMethod(p->mediaPlayer_, &QMediaPlayer::play);
}

void MediaManager::Stop()
{
   if (p->mediaPlayer_ == nullptr)
   {
      logger_->warn("Media player is not yet initialized");
      return;
   }

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

} // namespace scwx::qt::manager
