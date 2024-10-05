#pragma once

#include <scwx/qt/types/marker_types.hpp>

#include <string>

#include <QObject>

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

   size_t            marker_count();
   types::MarkerInfo get_marker(size_t index);
   types::MarkerInfo get_marker(const std::string& name);
   void              set_marker(size_t index, const types::MarkerInfo& marker);
   void set_marker(const std::string& name, const types::MarkerInfo& marker);
   void add_marker(const types::MarkerInfo& marker);
   void move_marker(size_t from, size_t to);

   static std::shared_ptr<MarkerManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
