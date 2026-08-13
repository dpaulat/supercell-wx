#pragma once

#include <scwx/awips/text_product_message.hpp>
#include <scwx/qt/types/text_event_key.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>

#include <boost/uuid/uuid.hpp>
#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class TextEventManager : public QObject
{
   Q_OBJECT

public:
   explicit TextEventManager();
   ~TextEventManager();

   size_t message_count(const types::TextEventKey& key) const;
   std::vector<std::shared_ptr<awips::TextProductMessage>>
   message_list(const types::TextEventKey& key) const;

   void LoadFile(const std::string& filename);
   void SelectTime(std::chrono::system_clock::time_point dateTime);

   static std::shared_ptr<TextEventManager> Instance();

signals:
   void AlertsRemoved(
      const std::unordered_set<types::TextEventKey,
                               types::TextEventHash<types::TextEventKey>>&
         keys);
   void AlertUpdated(const types::TextEventKey& key,
                     std::size_t                messageIndex,
                     boost::uuids::uuid         uuid);
   void BulkAlertLoadStarted();
   void BulkAlertLoadFinished();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
