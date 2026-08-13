#include <scwx/qt/map/map_perf.hpp>

#include <scwx/util/environment.hpp>
#include <scwx/util/logger.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

namespace scwx::qt::map
{

namespace
{

static const std::string logPrefix_ = "scwx::qt::map::map_perf";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

constexpr std::uint64_t kLogIntervalFrames = 120;

struct PanePerfTotals
{
   std::uint64_t frameCount_ {0};
   double        totalMs_ {0.0};
   double        mapLibreMs_ {0.0};
   double        basemapCopyMs_ {0.0};
   double        imguiMs_ {0.0};
   double        overlayMs_ {0.0};
   std::uint64_t mapLibreRenderedFrames_ {0};
   std::uint64_t basemapCopiedFrames_ {0};
   std::uint64_t followerFrames_ {0};
   std::uint64_t leaderFrames_ {0};
};

struct PerfState
{
   std::mutex                                      mutex_ {};
   std::unordered_map<std::size_t, PanePerfTotals> paneTotals_ {};
   PanePerfTotals                                  gridTotals_ {};
   std::uint64_t                                   recordedFrames_ {0};
   bool                                            csvInitialized_ {false};
   std::string                                     csvPath_ {};
};

PerfState& State()
{
   static PerfState state {};
   return state;
}

void MaybeInitCsvPath(PerfState& state)
{
   if (state.csvInitialized_)
   {
      return;
   }

   state.csvInitialized_  = true;
   const std::string path = scwx::util::GetEnvironment("SCWX_VULKAN_PERF_CSV");
   if (!path.empty())
   {
      state.csvPath_ = path;
   }
}

void AppendCsvRow(const PerfState& state, const PanePerfTotals& grid)
{
   if (state.csvPath_.empty() || grid.frameCount_ == 0)
   {
      return;
   }

   const double  frames = static_cast<double>(grid.frameCount_);
   std::ofstream out(state.csvPath_, std::ios::app);
   if (!out.is_open())
   {
      return;
   }

   if (out.tellp() == 0)
   {
      out << "timestamp_ms,grid_frames,total_ms,map_ms,basemap_copy_ms,"
             "imgui_ms,overlay_ms,map_render_frames,basemap_copy_frames,"
             "follower_frames,leader_frames\n";
   }

   const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

   out << nowMs << ',' << grid.frameCount_ << ',' << (grid.totalMs_ / frames)
       << ',' << (grid.mapLibreMs_ / frames) << ','
       << (grid.basemapCopyMs_ / frames) << ',' << (grid.imguiMs_ / frames)
       << ',' << (grid.overlayMs_ / frames) << ','
       << grid.mapLibreRenderedFrames_ << ',' << grid.basemapCopiedFrames_
       << ',' << grid.followerFrames_ << ',' << grid.leaderFrames_ << '\n';
}

void LogPaneSummary(const std::size_t paneId, const PanePerfTotals& pane)
{
   if (pane.frameCount_ == 0)
   {
      return;
   }

   const double frames = static_cast<double>(pane.frameCount_);
   logger_->info(
      "Vulkan perf pane={}: frames={} avg_total_ms={:.3f} avg_map_ms={:.3f} "
      "avg_basemap_copy_ms={:.3f} avg_imgui_ms={:.3f} avg_overlay_ms={:.3f} "
      "map_render_frames={} basemap_copy_frames={} follower_frames={} "
      "leader_frames={}",
      paneId,
      pane.frameCount_,
      pane.totalMs_ / frames,
      pane.mapLibreMs_ / frames,
      pane.basemapCopyMs_ / frames,
      pane.imguiMs_ / frames,
      pane.overlayMs_ / frames,
      pane.mapLibreRenderedFrames_,
      pane.basemapCopiedFrames_,
      pane.followerFrames_,
      pane.leaderFrames_);
}

void LogGridSummary(const PanePerfTotals& grid,
                    const std::size_t     activePaneCount,
                    const std::size_t     alertSegments)
{
   if (grid.frameCount_ == 0)
   {
      return;
   }

   const double frames = static_cast<double>(grid.frameCount_);
   logger_->info(
      "Vulkan perf grid: panes={} frames={} avg_total_ms={:.3f} "
      "avg_map_ms={:.3f} avg_basemap_copy_ms={:.3f} avg_imgui_ms={:.3f} "
      "avg_overlay_ms={:.3f} map_render_frames={} basemap_copy_frames={} "
      "follower_frames={} leader_frames={} alert_segments={}",
      activePaneCount,
      grid.frameCount_,
      grid.totalMs_ / frames,
      grid.mapLibreMs_ / frames,
      grid.basemapCopyMs_ / frames,
      grid.imguiMs_ / frames,
      grid.overlayMs_ / frames,
      grid.mapLibreRenderedFrames_,
      grid.basemapCopiedFrames_,
      grid.followerFrames_,
      grid.leaderFrames_,
      alertSegments);
}

void Accumulate(PanePerfTotals& totals, const MapFramePerfSample& sample)
{
   ++totals.frameCount_;
   totals.totalMs_ += sample.totalMs_;
   totals.mapLibreMs_ += sample.mapLibreMs_;
   totals.basemapCopyMs_ += sample.basemapCopyMs_;
   totals.imguiMs_ += sample.imguiMs_;
   totals.overlayMs_ += sample.overlayMs_;

   if (sample.mapLibreRendered_)
   {
      ++totals.mapLibreRenderedFrames_;
   }
   if (sample.basemapCopied_)
   {
      ++totals.basemapCopiedFrames_;
   }
   if (sample.basemapRole_ == BasemapPaneRole::Follower)
   {
      ++totals.followerFrames_;
   }
   else if (sample.basemapRole_ == BasemapPaneRole::Leader)
   {
      ++totals.leaderFrames_;
   }
}

} // namespace

bool MapPerfEnabled() noexcept
{
   return scwx::util::HasEnvironment("SCWX_VULKAN_PERF");
}

void RecordMapFramePerf(const MapFramePerfSample& sample) noexcept
{
   if (!MapPerfEnabled())
   {
      return;
   }

   auto&           state = State();
   std::lock_guard lock {state.mutex_};
   MaybeInitCsvPath(state);

   Accumulate(state.paneTotals_[sample.paneId_], sample);
   Accumulate(state.gridTotals_, sample);
   ++state.recordedFrames_;

   PanePerfTotals& pane = state.paneTotals_[sample.paneId_];
   if (pane.frameCount_ % kLogIntervalFrames == 0)
   {
      LogPaneSummary(sample.paneId_, pane);
      pane = PanePerfTotals {};
   }

   if (state.gridTotals_.frameCount_ % kLogIntervalFrames == 0)
   {
      LogGridSummary(state.gridTotals_,
                     state.paneTotals_.size(),
                     sample.alertSegmentCount_);
      AppendCsvRow(state, state.gridTotals_);
      state.gridTotals_ = PanePerfTotals {};
   }
}

void ResetMapPerfForTest() noexcept
{
   auto&           state = State();
   std::lock_guard lock {state.mutex_};
   state.paneTotals_.clear();
   state.gridTotals_     = PanePerfTotals {};
   state.recordedFrames_ = 0;
   state.csvInitialized_ = false;
   state.csvPath_.clear();
}

std::uint64_t MapPerfRecordedFrameCountForTest() noexcept
{
   auto&           state = State();
   std::lock_guard lock {state.mutex_};
   return state.recordedFrames_;
}

} // namespace scwx::qt::map
