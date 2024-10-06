#pragma once

#include <scwx/qt/types/marker_types.hpp>

#include <QObject>
#include <optional>

namespace scwx
{
namespace qt
{
namespace manager
{

class MarkerManager : public QObject
{
   Q_OBJECT

public:
   explicit MarkerManager();
   ~MarkerManager();

   size_t                   marker_count();
   std::optional<types::MarkerInfo> get_marker(size_t index);
   void set_marker(size_t index, const types::MarkerInfo& marker);
   void add_marker(const types::MarkerInfo& marker);
   void remove_marker(size_t index);
   void move_marker(size_t from, size_t to);

   static std::shared_ptr<MarkerManager> Instance();

signals:
   void MarkersUpdated();
   void MarkerAdded();
   void MarkerRemoved(size_t index);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
