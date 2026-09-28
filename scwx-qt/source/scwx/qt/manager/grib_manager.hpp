#pragma once

#include <scwx/qt/map/grib_frame_info.hpp>
#include <scwx/qt/types/map_types.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <QObject>

namespace scwx::provider
{
class AwsNexradDataProvider;
} // namespace scwx::provider

namespace scwx::qt::manager
{

// Owns provider::MrmsDataProvider/RtmaDataProvider and polls/fetches on
// demand -- an earlier prototype of this same job was a standalone shell
// script polling S3 directly and shelling out to decode_grib by hand;
// this replaced it with real, in-process C++.
//
// Deliberately simpler than manager::ProviderManager (which this reuses
// AwsNexradDataProvider underneath, but not ProviderManager itself --
// ProviderManager's constructor is hard-tied to RadarProductManager* as
// its signal-owning "self"). This class mirrors ProviderManager's
// adaptive-poll *pattern* without literally being one: single product, no
// per-site fan-out, QTimer instead of a boost::asio::steady_timer for the
// live-poll cadence -- though a boost::asio::thread_pool now *is* used for
// background fetch jobs (see below), the same async pattern
// TimelineManager itself uses.
//
// GribProductLayer subscribes to this manager's FrameReady signal (and
// keeps a slower file-mtime poll as a fallback) to know when to reload --
// no direct coupling beyond that; instantiating this manager is the only
// integration point (see GribProductLayer::Initialize()).
//
// Subscribes to TimelineManager for both time and animation state: live
// mode keeps polling for the latest file; archive mode fetches the file
// nearest the user's selected time; and when the animation loop starts
// playing, PrefetchLoopRange() downloads everything the loop will need in
// the background ahead of time, so per-step playback doesn't block on
// individual downloads (that per-step blocking was the actual cause of
// "sputter and lurch" during archive playback for a data source this
// slow to fetch -- radar's own async loader doesn't have this problem
// since it's much smaller data per file). All of this reuses
// AwsNexradDataProvider's own FindKey()/ListObjects()/cache machinery
// rather than re-deriving it independently.
//
// Fetches only ever cache the *downloaded* GRIB2 payload on disk, not the
// decoded frame -- decode_grib's own runtime is ~30ms (cheap), while a
// decoded MRMS composite reflectivity frame is ~98MB (not cheap to keep
// dozens of copies of for a whole loop range). A cached download is
// reused across every product/field that shares the same S3 key (all of
// RTMA's fields, since they're bundled in one file) as well as across
// repeated loop playthroughs.
//
// One instance per map::GribCategory (see Instance()) -- Mrms/Rtma/Rrfs
// are otherwise identical in behavior, just backed by a different curated
// product table, so all three can be fetching/displaying independently at
// once.
//
// Supports selecting a small curated set of products at runtime, scoped
// to this instance's own category (see ProductNames()) rather than being
// fixed to composite reflectivity -- and unlike a single "current
// product", more than one can be active simultaneously (see
// SetProductActive()), each with its own frame file
// (map::GetGribFramePath(category, index)). GribDockWidget's per-category
// checkable dropdown drives this.
class GribManager : public QObject
{
   Q_OBJECT

public:
   explicit GribManager(map::GribCategory category);
   ~GribManager();

   GribManager(const GribManager&)            = delete;
   GribManager& operator=(const GribManager&) = delete;
   GribManager(GribManager&&)                 = delete;
   GribManager& operator=(GribManager&&)      = delete;

   static std::shared_ptr<GribManager> Instance(map::GribCategory category);

   // Display names for this instance's category's curated product list,
   // in display order -- what GribDockWidget's checkable dropdown
   // populates itself from.
   [[nodiscard]] std::vector<std::string> ProductNames() const;

   // Multi-select: each active product is fetched/decoded/rendered
   // independently of the others, into its own frame file (see
   // map::GetGribFramePath(category, index)) -- several products active
   // at once within one category, built one level narrower than a full
   // per-pane version would be (per manager instance, not yet per pane).
   // At least one product is always active; a request to
   // deactivate the last one is refused, not silently ignored -- there
   // must always be something for CurrentProductIndex()/GribProductLayer
   // to fall back on. No-op if displayName isn't in ProductNames(), or
   // already in the requested state.
   void SetProductActive(const std::string& displayName, bool active);
   [[nodiscard]] bool IsProductActive(const std::string& displayName) const;
   [[nodiscard]] std::vector<std::string> ActiveProductNames() const;

   // The lowest-indexed active product -- what GribProductLayer's own
   // single-frame renderer shows (see class comment: rendering multiple
   // active products at once on the map is a separate, not-yet-built
   // piece; this manager's fetch/decode side supports it today, the
   // render side doesn't). Also what CurrentProductName()/FormatValue()
   // below report on.
   [[nodiscard]] std::size_t CurrentProductIndex() const;
   [[nodiscard]] std::string CurrentProductName() const;

   // Formats a raw decoded value for the current (see CurrentProductIndex)
   // product as "<value> <unit>", converting to whatever the user has set
   // in Settings > Units first when the product's physical quantity has a
   // matching setting (e.g. Kelvin -> temperature_units(), m/s ->
   // speed_units()) -- same idea as RadarProductLayer's own tooltip
   // converting distance/height. Products with no such quantity (dBZ,
   // MRMS's raw rotation track units) are shown natively, unconverted.
   [[nodiscard]] std::string FormatValue(float rawValue) const;

   // RRFS-only forecast-hour/cycle selection (see RrfsDataProvider's own
   // SetCycle()/SetForecastHour()) -- a no-op (logged) on an Mrms/Rtma
   // instance, which have no forecast-hour axis at all. Named with an
   // explicit "Rrfs" prefix (unlike every other method here, which is
   // already scoped to this instance's category implicitly) specifically
   // so a call against the wrong instance reads as suspicious at the call
   // site rather than silently compiling as if it were generic.
   //
   // Independent of the live/archive timeline state every other product
   // category shares (see HandleLiveStateUpdated/HandleSelectedTimeUpdated)
   // -- RRFS's own cycle/hour selection is a second, orthogonal axis a
   // dedicated run/hour picker UI drives, not the main map timeline
   // scrubber (RrfsDataProvider::GetPrefix() only reads its own resolved
   // cycle/hour, never the `date` a timeline scrub would pass down, so
   // scrubbing the main timeline can't select a different RRFS forecast
   // hour anyway -- out of scope here, see RrfsDataProvider's class
   // comment on why one instance is scoped to exactly one file at a time).
   // Applies to every currently-active product's provider at once (each
   // active product owns its own provider instance -- see
   // Impl::providers_) and to any provider created later by
   // SetProductActive(), and immediately fetches the newly-selected
   // (cycle, forecast hour) via the same archive-fetch path
   // FetchArchiveFrame() already uses, rather than waiting for the next
   // live poll tick.
   void SetRrfsCycle(std::chrono::system_clock::time_point cycleTime);
   void UseLatestRrfsCycle();
   [[nodiscard]] bool IsUsingLatestRrfsCycle() const;

   // The cycle currently targeted -- either the SetRrfsCycle() override, or
   // (while following latest) whatever RrfsDataProvider::CurrentCycle()
   // itself resolves that to right now, so a run picker can show what
   // "latest" actually means.
   [[nodiscard]] std::chrono::system_clock::time_point CurrentRrfsCycle() const;

   // 0 (default) selects F000. Not itself a statement about live-vs-fixed
   // cycle tracking -- e.g. "latest cycle, hour 6" (UseLatestRrfsCycle()
   // plus SetRrfsForecastHour(6)) is a valid combination that keeps
   // tracking forward as newer cycles publish, same hour offset each time.
   void              SetRrfsForecastHour(int hour);
   [[nodiscard]] int RrfsForecastHour() const;

   // RrfsDataProvider::MaxForecastHourForCycle(CurrentRrfsCycle()) -- 84 or
   // 18 depending on the targeted cycle's own hour -- exposed here too so
   // an hour slider can size itself without reaching into a private
   // provider instance.
   [[nodiscard]] int MaxRrfsForecastHour() const;

   // Bounds both PrefetchRrfsForecastHourRange() and, via
   // GribDockWidget's own Play/pause loop, which hour the animation wraps
   // back to -- letting the user watch (and only pay the prefetch/cache
   // cost for) a deliberately narrower window, e.g. "just the next 6
   // hours," instead of always the full cycle. `endHour` < 0 means
   // "unset" -- reverts to the original, unbounded [0,
   // MaxRrfsForecastHour()] behavior; anything else is clamped to
   // [startHour, MaxRrfsForecastHour()] the next time either method
   // actually reads it (CurrentRrfsCycle() can still change afterward in
   // a way that changes the valid range, same reasoning as
   // SetForecastHour()'s own doc).
   void              SetRrfsLoopRange(int startHour, int endHour);
   [[nodiscard]] int RrfsLoopStartHour() const;
   [[nodiscard]] int RrfsLoopEndHour() const; // -1 if unset

   // Downloads (if not already cached) every forecast hour in
   // [RrfsLoopStartHour(), RrfsLoopEndHour() or MaxRrfsForecastHour() if
   // unset] for the current RRFS cycle, in the background, ahead of
   // GribDockWidget's own Play/pause loop reaching each one -- the
   // RRFS-forecast-hour-axis counterpart to PrefetchLoopRange(), called
   // from the same kind of place (the Play button being pressed) but not
   // funneled through HandleAnimationStateUpdated, since the RRFS hour
   // slider's animation loop is its own local QTimer in GribDockWidget, not
   // TimelineManager's. Uses RrfsDataProvider::BuildKey() (no listing, no
   // provider-state mutation) rather than SetForecastHour()+FindKey() in a
   // loop, specifically so this can queue every hour's key up front without
   // racing the one live provider instance that's actually driving the
   // current on-screen selection, or blocking the calling (GUI) thread on
   // dozens of sequential S3 listing calls.
   //
   // Real cost, not silently absorbed: each hour is its own ~320MB object,
   // so a full 6-hourly cycle's *unbounded* 84-hour range is up to ~27GB
   // queued at once -- PruneDownloadCache()'s eviction cap is sized (see
   // its own kMaxCacheSizeBytes_ doc) with headroom above this exact worst
   // case. SetRrfsLoopRange() exists specifically so this worst case is
   // opt-in, not the only option -- a bounded loop (e.g. 0-6h) costs
   // proportionally less. Current product only, same documented scoping
   // limitation as PrefetchLoopRange().
   void PrefetchRrfsForecastHourRange();

   // Nbm-only cycle/forecast-hour selection -- same shape and reasoning as
   // the Rrfs block above (a second, orthogonal axis from the main
   // timeline, applied to every active provider, fetched immediately
   // rather than waiting for a poll tick that -- for Nbm -- would never
   // come anyway, see Poll()'s own comment on why it skips this category
   // entirely). No loop-range/prefetch equivalent: each Nbm product's own
   // cached download is one range-fetched field (~1-2MB), nowhere near
   // the cache-budget pressure a whole RRFS forecast-hour prefetch
   // creates, so there's been no need for one yet.
   //
   // SetNbmForecastHour() snaps `hour` to the nearest real, fetchable
   // hour for the currently-targeted cycle before storing it (see
   // NbmDataProvider::SnapForecastHour()) -- NBM's own forecast-hour step
   // is non-uniform (hourly, then 3-hourly, then 6-hourly) for the
   // 6-hourly "extended" cycles, unlike RRFS's uniform step, so a caller
   // driving this from a linear slider needs the snap to land on
   // something that actually exists.
   void SetNbmCycle(std::chrono::system_clock::time_point cycleTime);
   void UseLatestNbmCycle();
   [[nodiscard]] bool IsUsingLatestNbmCycle() const;
   [[nodiscard]] std::chrono::system_clock::time_point CurrentNbmCycle() const;
   void              SetNbmForecastHour(int hour);
   [[nodiscard]] int NbmForecastHour() const;
   [[nodiscard]] int MaxNbmForecastHour() const;

signals:
   // Emitted once a requested frame has actually been decoded and applied
   // to GetGribFramePath(category, productIndex) -- may fire from a
   // background fetch thread (Qt's queued cross-thread delivery makes
   // this safe to connect to from GribProductLayer's own, different
   // thread). GribProductLayer only ever sets a dirty flag in response
   // (and only for productIndex == CurrentProductIndex(), since it
   // doesn't render the others yet), applying it in Render(), same as
   // its existing palette-changed handling.
   void FrameReady(std::size_t productIndex);

private:
   void Poll();
   void HandleLiveStateUpdated(bool isLive);
   void
   HandleSelectedTimeUpdated(std::chrono::system_clock::time_point dateTime);
   void HandleAnimationStateUpdated(types::AnimationState state);

   // Given an archive time the user selected, resolves the GRIB2 file
   // nearest that time and requests it, for every active product.
   // Returns true if at least one request was made (not that any have
   // necessarily completed yet -- see RequestFrame()).
   bool FetchArchiveFrame(std::chrono::system_clock::time_point time);

   // FetchArchiveFrame()'s own per-product work, factored out since it
   // has to repeat identically for each active product (each may have a
   // different provider -- see Impl::providers_).
   bool FetchArchiveFrameForProduct(std::size_t productIndex,
                                    std::chrono::system_clock::time_point time);

   // SHIP's own two-file (2dfld + prslev) selection -- FetchArchiveFrame
   // ForProduct() dispatches here instead of its normal one-key path
   // whenever a product's derivedIndex == "ship" (see ComputeShip's own
   // doc in decode_grib.cpp for why it needs both files at once).
   // Resolves both current keys, queues whichever isn't cached yet (see
   // QueueShipInput()), and checks whether both already are (see
   // ApplyShipIfReady()).
   void FetchShipSelection(std::size_t productIndex);

   // Re-checks whether SHIP's *current* selection (recomputed fresh from
   // the provider's own live cycle/forecast-hour state, not trusted from
   // whatever triggered this call) has both its inputs on disk yet and,
   // if so, decodes via ApplyShipDownload(). Called once synchronously
   // from FetchShipSelection() (covers "both were already cached") and
   // again from each QueueShipInput() job's own completion -- a stale
   // completion (the selection moved on while a download was in flight)
   // naturally finds the *new* combination still incomplete and applies
   // nothing.
   void ApplyShipIfReady(std::size_t productIndex);

   // Downloads one of SHIP's two inputs on the background thread pool if
   // not already in flight, then calls ApplyShipIfReady() once done.
   // Mirrors QueueDownload()'s own shape but doesn't call it directly --
   // that function's own completion unconditionally applies a *single*
   // key via ApplyCachedDownload(), which would invoke decode_grib with
   // the wrong CLI form for SHIP's two-input mode.
   void QueueShipInput(std::size_t productIndex, const std::string& key);

   // Decodes SHIP from its two already-downloaded inputs and atomically
   // replaces productIndex's own frame file -- ApplyCachedDownload()'s
   // two-input counterpart, kept separate rather than extending that
   // function's signature since every other product only ever has one
   // input. Emits FrameReady(productIndex) on success.
   bool ApplyShipDownload(std::size_t        productIndex,
                          const std::string& key2dfld,
                          const std::string& keyPrslev);

   // Applies this instance's own stored Nbm cycle/forecast-hour selection
   // to one provider -- same shape and reason as SyncRrfsProviderState().
   // Nbm-only; caller must already know category_ == Nbm.
   void SyncNbmProviderState(provider::AwsNexradDataProvider& provider) const;

   // SetNbmCycle()/SetNbmForecastHour()/UseLatestNbmCycle()'s shared "now
   // go fetch that" tail -- same shape as FetchRrfsSelection(), calling
   // FetchArchiveFrame() (whose per-product dispatch, for Nbm, ignores the
   // time argument and resolves each product's own key from its own
   // provider state instead -- see FetchNbmSelectionForProduct()).
   void FetchNbmSelection();

   // Nbm's own per-product dispatch -- FetchArchiveFrameForProduct()
   // routes here instead of its normal listing-based path whenever
   // category_ == Nbm, since a key alone doesn't say which *field* to
   // download out of NBM's own multi-field per-cycle file (see
   // ProductConfig::nbmParameter/nbmLevel/nbmQualifier's own doc).
   // Resolves the current key, checks whether this product's own
   // per-field cache entry already exists, and either applies it
   // synchronously or queues a download (see QueueNbmDownload()).
   void FetchNbmSelectionForProduct(std::size_t productIndex);

   // Downloads one Nbm product's own field on the background thread pool
   // (if not already in flight) via NbmDataProvider::FetchField() (the
   // idx-based range fetch), then applies it via the existing
   // ApplyCachedDownload() -- unlike SHIP, one product needs only one
   // input, so no dedicated Apply* counterpart is needed here; the
   // existing single-input decode path already fits once the field is on
   // disk under its own per-field cache key. `key` is the real S3 object
   // key (what FetchField() range-fetches from); `cacheKey` is that key
   // suffixed with this product's own shortName (what CachedDownloadPath()
   // uses -- several products share one `key`, so the plain key alone
   // can't be the cache path).
   void QueueNbmDownload(std::size_t        productIndex,
                        const std::string& key,
                        const std::string& cacheKey);

   // Downloads (if not already cached on disk) and decodes everything the
   // current animation loop range will need, in the background, ahead of
   // playback reaching it -- see class comment. Only the current (primary)
   // product for now, not every active one -- prefetching the full active
   // set is a reasonable follow-up, not done here.
   void PrefetchLoopRange();

   // Applies this instance's own stored RRFS cycle/forecast-hour selection
   // (see SetRrfsCycle()/SetRrfsForecastHour()) to one provider -- shared
   // by the propagate-to-every-active-provider loop those setters run and
   // by SetProductActive(), which needs to bring a freshly-constructed
   // provider (MakeProvider() always starts one at auto/latest/F000
   // defaults) in line with whatever selection is already in effect.
   // Rrfs-only; caller must already know category_ == Rrfs.
   void SyncRrfsProviderState(provider::AwsNexradDataProvider& provider) const;

   // SetRrfsCycle()/SetRrfsForecastHour()/UseLatestRrfsCycle()'s shared
   // "now go fetch that" tail -- computes the targeted (cycle + forecast
   // hour) valid time from the primary active product's own provider (every
   // active provider was just synced to the same selection, so any of them
   // would resolve identically) and fetches it for every active product via
   // FetchArchiveFrame(), the same mechanism archive-mode timeline
   // scrubbing already uses.
   void FetchRrfsSelection();

   // Records `key` as the frame `productIndex` currently wants. If it's
   // already cached on disk, applies it immediately (decode is cheap,
   // done synchronously right here); otherwise queues a background
   // download and returns without blocking -- whatever's currently
   // displayed for this product stays up until the download completes
   // and ApplyCachedDownload() runs.
   void RequestFrame(std::size_t productIndex, const std::string& key);

   // Downloads `key` on the background thread pool if this (productIndex,
   // key) pair isn't already in flight. Products sharing a category
   // (RTMA/RRFS bundle every field into one file) each get their own
   // in-flight entry even for the same key, since each still needs its
   // own decode once the shared bytes are down -- a real, if narrow,
   // redundant-download cost when two products are both newly activated
   // at the same uncached moment, traded for not silently dropping one
   // product's decode the way a key-only dedup would. Applies the result
   // once complete only if `key` is still what productIndex wants (a
   // request may have been superseded by further playback/scrubbing
   // while this was in-flight) -- otherwise leaves it cached on disk for
   // potential reuse without disrupting whatever's currently displayed.
   void QueueDownload(std::size_t productIndex, const std::string& key);

   // Decodes an already-downloaded (cached) GRIB2 payload for `key` using
   // the given color range/shortName (a snapshot of the ProductConfig this
   // fetch was requested for -- ProductConfig itself is file-local to
   // grib_manager.cpp, not part of this class's interface), and atomically
   // replaces productIndex's own frame file. Emits FrameReady(productIndex)
   // on success. Safe to call from any thread.
   bool ApplyCachedDownload(std::size_t        productIndex,
                            const std::string& key,
                            const std::string& shortName,
                            float              colorOffset,
                            float              colorScale,
                            float              noDataThreshold,
                            float              contourInterval,
                            const std::string& derivedIndex,
                            const std::string& typeOfLevel,
                            long               topLevel,
                            long               bottomLevel,
                            long               startStep,
                            long               lengthOfTimeRange);

   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace scwx::qt::manager
