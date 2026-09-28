#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <QObject>

namespace scwx::qt::manager
{

// A single, app-wide place background fetch/decode activity reports
// through, so the status bar has one thing to subscribe to regardless of
// which manager (GribManager/WindBarbManager/HodographManager, so far)
// is actually doing the work -- answers the "AutoCAD-style status bar
// busy indicator" idea noted earlier in this project, triggered
// concretely by RRFS's own ~320MB-per-fetch cost having zero visible
// user feedback (a slow S3 fetch, a stuck decode_grib call, or a silent
// failure all look identical today: nothing changes on screen).
//
// Deliberately narrow: reports on *download* activity specifically (byte
// counts), not a general logging console -- the earlier idea's other
// open questions (tail the logger vs. a curated stream; how multi-pane
// reconciles) don't apply here, since every reporting manager is already
// an app-wide singleton (not per-pane) and reports are structured
// (id/description/bytes), not raw log lines a console would need to
// filter/scroll.
class StatusManager : public QObject
{
   Q_OBJECT

public:
   ~StatusManager();

   StatusManager(const StatusManager&)            = delete;
   StatusManager& operator=(const StatusManager&) = delete;
   StatusManager(StatusManager&&)                 = delete;
   StatusManager& operator=(StatusManager&&)      = delete;

   static std::shared_ptr<StatusManager> Instance();

   // Reports (or updates) one in-progress download, keyed by `id` (e.g.
   // "grib-rrfs-0", "hodograph") -- calling again for the same id updates
   // its entry rather than adding a second one. totalBytes <= 0 means
   // "unknown total" (shown without a percentage/fraction).
   void ReportProgress(const std::string& id,
                       const std::string& description,
                       std::int64_t       bytesReceived,
                       std::int64_t       totalBytes);

   // Clears one id's entry -- call once its download finishes
   // (successfully or not, cached-hit or real fetch); an id that's never
   // reported never appears in CurrentStatusText() at all.
   void ReportComplete(const std::string& id);

   // The single line the status bar should currently show, or an empty
   // string when nothing is in progress. When more than one id is
   // active, shows the most recently *updated* one plus a "(+N more)"
   // suffix -- a simple "what's the freshest thing happening" readout,
   // not an attempt at a multi-line console.
   [[nodiscard]] std::string CurrentStatusText() const;
   [[nodiscard]] bool        IsBusy() const;

signals:
   // Emitted whenever CurrentStatusText()/IsBusy() may have changed --
   // may fire from any thread that calls ReportProgress()/
   // ReportComplete() (every current caller's own download runs on a
   // background thread pool) -- Qt's automatic queued cross-thread
   // delivery makes connecting to this from the GUI thread safe. Also
   // fires on a periodic internal timer purely to notice *stale* entries
   // going away (see ReportProgress()'s own doc on why some callers have
   // no other way to signal "done") -- a query method noticing an entry
   // is stale wouldn't, on its own, prompt anything to re-query and see
   // the now-cleared text; this timer is what actually does that.
   void StatusChanged();

private:
   explicit StatusManager();

   void CheckForStaleEntries();

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
