#pragma once

#include <scwx/qt/types/media_types.hpp>

#include <memory>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class MediaManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(MediaManager)

public:
   explicit MediaManager();
   ~MediaManager();

   void Play(types::AudioFile media);
   void Play(const std::string& mediaPath);
   void Stop();

   static std::shared_ptr<MediaManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
