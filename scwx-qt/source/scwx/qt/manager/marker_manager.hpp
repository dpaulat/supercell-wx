#pragma once

#include <scwx/qt/types/marker_types.hpp>

#include <istream>
#include <optional>
#include <ostream>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

typedef void MarkerForEachFunc(const types::MarkerInfo&);
class MarkerManager : public QObject
{
   Q_OBJECT

public:
   explicit MarkerManager();
   ~MarkerManager();

   void ReadMarkerSettings(std::istream& is);
   void WriteMarkerSettings(std::ostream& os);

   size_t                           marker_count();
   std::optional<types::MarkerInfo> get_marker(types::MarkerId id);
   std::optional<size_t>            get_index(types::MarkerId id);
   void set_marker(types::MarkerId id, const types::MarkerInfo& marker);
   types::MarkerId add_marker(const types::MarkerInfo& marker);
   void            remove_marker(types::MarkerId id);
   void            move_marker(size_t from, size_t to);

   void add_icon(const std::string& name, bool startup = false);
   std::optional<types::MarkerIconInfo> get_icon(const std::string& name);
   const std::unordered_map<std::string, types::MarkerIconInfo> get_icons();

   void for_each(std::function<MarkerForEachFunc> func);

   // Only use for testing
   void set_marker_settings_path(const std::string& path);

   static std::shared_ptr<MarkerManager> Instance();
   static const std::string&             getDefaultIconName();

signals:
   void MarkersInitialized(size_t count);
   void MarkersUpdated();
   void MarkerChanged(types::MarkerId id);
   void MarkerAdded(types::MarkerId id);
   void MarkerRemoved(types::MarkerId id);

   void IconsReady();
   void IconAdded(std::string name);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
