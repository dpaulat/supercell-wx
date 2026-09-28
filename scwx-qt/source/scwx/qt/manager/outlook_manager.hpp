#pragma once

#include <memory>
#include <string>

#include <QObject>

namespace scwx::qt::manager
{

// PlacefileManager category this manager's entries register under (see
// PlacefileManager::AddUrl's category parameter) -- ui::PlacefileDialog's
// secondary "Outlooks" tab filters on this exact string, so it's a
// function rather than a hardcoded literal duplicated in that file.
[[nodiscard]] const std::string& OutlookPlacefileCategory();

// Periodically fetches a curated list of SPC/WPC outlook GeoJSON sources
// (Day 1 categorical/tornado/wind/hail/fire-weather from SPC, plus WPC's
// Excessive Rainfall Outlook), converts each to Place File text
// (scwx::gr::ConvertOutlookGeoJsonToPlacefile), and registers the result
// as a built-in entry in PlacefileManager -- so every source gets
// rendering, persistent enable/disable, and a settings UI entirely for
// free from the existing placefile pipeline. One manager, one fetch list
// (see the .cpp's Sources()) -- a new outlook source is another entry
// there, not a new manager.
class OutlookManager : public QObject
{
   Q_OBJECT

public:
   explicit OutlookManager();
   ~OutlookManager();

   OutlookManager(const OutlookManager&)            = delete;
   OutlookManager& operator=(const OutlookManager&) = delete;
   OutlookManager(OutlookManager&&)                 = delete;
   OutlookManager& operator=(OutlookManager&&)      = delete;

   static std::shared_ptr<OutlookManager> Instance();

private:
   void Poll();

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
