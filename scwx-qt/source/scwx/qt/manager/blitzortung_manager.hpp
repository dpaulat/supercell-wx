#pragma once

#include <scwx/provider/blitzortung_data.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <QObject>
#include <QTimer>

namespace scwx::provider
{
class BlitzortungProvider;
} // namespace scwx::provider

namespace scwx::qt::manager
{

struct TimedStrikeData
{
   scwx::provider::StrikeData            strike_;
   std::chrono::steady_clock::time_point receiptTime_;
};

class BlitzortungManager : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_MOVE(BlitzortungManager)

public:
   explicit BlitzortungManager();
   ~BlitzortungManager();

   void Start();
   void Stop();
   bool IsActive() const;

   std::vector<TimedStrikeData> GetActiveStrikes() const;

   void InjectTestStrike(double lat, double lon);

   static BlitzortungManager& Instance();

signals:
   void StrikesUpdated();
   void ConnectionStatusChanged(bool connected);

private slots:
   void OnTimerTick();

private:
   void OnNewStrike(const provider::StrikeData& strike);

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
