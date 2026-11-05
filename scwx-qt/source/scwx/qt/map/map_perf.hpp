#pragma once

#include <scwx/qt/map/map_basemap_share.hpp>

#include <cstddef>
#include <cstdint>

namespace scwx::qt::map
{

/** Enabled when {@code SCWX_VULKAN_PERF} is set in the environment. */
[[nodiscard]] bool MapPerfEnabled() noexcept;

struct MapFramePerfSample
{
   std::size_t     paneId_ {0};
   double          totalMs_ {0.0};
   double          mapLibreMs_ {0.0};
   double          basemapCopyMs_ {0.0};
   double          imguiMs_ {0.0};
   double          overlayMs_ {0.0};
   bool            mapLibreRendered_ {false};
   bool            basemapCopied_ {false};
   BasemapPaneRole basemapRole_ {BasemapPaneRole::Independent};
   std::size_t     overlayLayerCount_ {0};
   std::size_t     alertSegmentCount_ {0};
};

/**
 * @brief Records one rendered frame and periodically logs aggregate stats.
 *
 * Per-pane averages log every {@code kLogIntervalFrames} samples for that pane.
 * Grid summary (all panes) logs on the same interval using combined totals.
 *
 * Optional CSV append when {@code SCWX_VULKAN_PERF_CSV} points to a file path.
 */
void RecordMapFramePerf(const MapFramePerfSample& sample) noexcept;

/** Resets counters (tests only). */
void ResetMapPerfForTest() noexcept;

[[nodiscard]] std::uint64_t MapPerfRecordedFrameCountForTest() noexcept;

} // namespace scwx::qt::map
