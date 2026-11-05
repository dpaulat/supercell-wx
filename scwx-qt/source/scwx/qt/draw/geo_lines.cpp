#include <scwx/qt/draw/geo_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>
#include <utility>

#include <boost/unordered/unordered_flat_set.hpp>
#include <glm/glm.hpp>
#include <units/angle.h>

#include <rhi/qrhi.h>

namespace scwx
{
namespace qt
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::draw::geo_lines";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t kNumRectangles        = 1;
static constexpr size_t kNumTriangles         = kNumRectangles * 2;
static constexpr size_t kVerticesPerTriangle  = 3;
static constexpr size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr size_t kPointsPerVertex      = 20;
static constexpr size_t kLineBufferLength_ =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;

// Threshold, start time, end time, displayed
static constexpr std::size_t kIntegersPerVertex_ = 4;
static constexpr std::size_t kIntegerBufferLength_ =
   kNumTriangles * kVerticesPerTriangle * kIntegersPerVertex_;

struct GeoLineDrawItem : types::EventHandler
{
   bool                                        visible_ {true};
   units::length::nautical_miles<double>       threshold_ {};
   std::chrono::sys_time<std::chrono::seconds> startTime_ {};
   std::chrono::sys_time<std::chrono::seconds> endTime_ {};

   boost::gil::rgba32f_pixel_t  modulate_ {1.0f, 1.0f, 1.0f, 1.0f};
   boost::gil::rgba32f_pixel_t  highlightColor_ {};
   boost::gil::rgba32f_pixel_t  borderColor_ {};
   float                        latitude1_ {};
   float                        longitude1_ {};
   float                        latitude2_ {};
   float                        longitude2_ {};
   float                        width_ {5.0};
   float                        strokeLineHalf_ {};
   float                        strokeHighlightHalf_ {};
   float                        strokeBorderHalf_ {};
   bool                         strokeEnabled_ {false};
   units::angle::degrees<float> angle_ {};
   std::string                  hoverText_ {};
   GeoLines::HoverCallback      hoverCallback_ {nullptr};
   size_t                       lineIndex_ {0};

   /// What is attached to one end of a line (see GeoLines::Impl::ResolveJoins)
   struct EndJoin
   {
      enum class Kind : std::uint8_t
      {
         /// Nothing else ends at this point: plain butt end
         Open,
         /// Other lines end here, but they cannot be joined with this line
         Blocked,
         /// Exactly one other, compatible line ends here: mitered join
         Joined
      };

      Kind        kind_ {Kind::Open};
      std::size_t other_ {0};
      /// True if the joined end of the other line is its second endpoint
      bool otherAtEnd_ {false};

      bool operator==(const EndJoin&) const = default;
   };

   EndJoin joinStart_ {};
   EndJoin joinEnd_ {};
};

struct GeoLineHoverEntry
{
   std::shared_ptr<GeoLineDrawItem> di_;

   glm::vec2 p1_;
   glm::vec2 p2_;
   glm::vec2 otl_;
   glm::vec2 otr_;
   glm::vec2 obl_;
   glm::vec2 obr_;
};

class GeoLines::Impl
{
public:
   explicit Impl(std::shared_ptr<render::RenderContext> context) :
       context_ {context}
   {
   }

   ~Impl() {}

   void BufferLine(const std::shared_ptr<const GeoLineDrawItem>& di);
   void Update();
   void UpdateBuffers();
   void UpdateModifiedLineBuffers();
   /// Finds which line endpoints meet exactly one other compatible line, so
   /// consecutive segments (polygon outlines, polylines) can share a mitered
   /// corner. Returns the indices of lines whose joins changed.
   std::vector<std::size_t> ResolveJoins();
   void UpdateSingleBuffer(const std::shared_ptr<GeoLineDrawItem>& di,
                           std::vector<float>&                     linesBuffer,
                           std::vector<std::int32_t>&             integerBuffer,
                           std::unordered_map<std::shared_ptr<GeoLineDrawItem>,
                                              GeoLineHoverEntry>& hoverLines);

   std::shared_ptr<render::RenderContext> context_;

   bool visible_ {true};
   bool dirty_ {false};
   bool thresholded_ {false};
   /// Line geometry or style changed since ResolveJoins() last ran
   bool joinsDirty_ {true};

   boost::unordered_flat_set<std::shared_ptr<GeoLineDrawItem>> dirtyLines_ {};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex lineMutex_ {};

   std::vector<std::shared_ptr<GeoLineDrawItem>> currentLineList_ {};
   std::vector<std::shared_ptr<GeoLineDrawItem>> newLineList_ {};

   std::vector<float>        currentLinesBuffer_ {};
   std::vector<std::int32_t> currentIntegerBuffer_ {};
   std::vector<float>        newLinesBuffer_ {};
   std::vector<std::int32_t> newIntegerBuffer_ {};

   std::unordered_map<std::shared_ptr<GeoLineDrawItem>, GeoLineHoverEntry>
      currentHoverLines_ {};
   std::unordered_map<std::shared_ptr<GeoLineDrawItem>, GeoLineHoverEntry>
      newHoverLines_ {};

   struct GeoRendererCacheEntry
   {
      std::unique_ptr<render::RhiGeoColoredGeometry> renderer_ {};
      bool                                           geometryUploaded_ {false};
      std::uint64_t renderTargetGeneration_ {0};
   };

   std::unordered_map<QRhi*, GeoRendererCacheEntry> geoRendererByRhi_ {};

   void InvalidateGeometryUploads();
   void EnsureGeoRenderer(render::RhiVulkanOverlayResources& resources,
                          QRhiCommandBuffer*                 commandBuffer);
};

GeoLines::GeoLines(std::shared_ptr<render::RenderContext> context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
GeoLines::~GeoLines() = default;

GeoLines::GeoLines(GeoLines&&) noexcept            = default;
GeoLines& GeoLines::operator=(GeoLines&&) noexcept = default;

void GeoLines::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void GeoLines::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void GeoLines::Initialize()
{
   p->dirty_ = true;
}

void GeoLines::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

void GeoLines::Impl::InvalidateGeometryUploads()
{
   for (auto& [rhi, entry] : geoRendererByRhi_)
   {
      (void) rhi;
      entry.geometryUploaded_ = false;
   }
}

void GeoLines::Impl::EnsureGeoRenderer(
   render::RhiVulkanOverlayResources& resources,
   QRhiCommandBuffer*                 commandBuffer)
{
   if (resources.rhi == nullptr || resources.renderTarget == nullptr)
   {
      return;
   }

   GeoRendererCacheEntry& entry = geoRendererByRhi_[resources.rhi];

   if (entry.renderTargetGeneration_ != resources.renderTargetGeneration)
   {
      if (entry.renderer_ != nullptr)
      {
         entry.renderer_->Shutdown();
      }
      entry.renderer_ = std::make_unique<render::RhiGeoColoredGeometry>();
      entry.renderTargetGeneration_ = resources.renderTargetGeneration;
      entry.geometryUploaded_       = false;
   }

   if (entry.renderer_ == nullptr)
   {
      entry.renderer_ = std::make_unique<render::RhiGeoColoredGeometry>();
      entry.renderTargetGeneration_ = resources.renderTargetGeneration;
   }

   if (!entry.renderer_->IsInitialized())
   {
      entry.renderer_->Initialize(
         resources.rhi, resources.renderTarget, commandBuffer);
   }
}

void GeoLines::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool /* textureAtlasChanged */)
{
   if (!p->visible_)
   {
      return;
   }

   std::unique_lock lock {p->lineMutex_};

   if (p->currentLineList_.empty() || p->currentLinesBuffer_.empty())
   {
      return;
   }

   if (!p->dirtyLines_.empty())
   {
      p->UpdateModifiedLineBuffers();
   }

   p->EnsureGeoRenderer(resources, commandBuffer);

   const auto entryIt = p->geoRendererByRhi_.find(resources.rhi);
   if (entryIt == p->geoRendererByRhi_.end() ||
       entryIt->second.renderer_ == nullptr ||
       !entryIt->second.renderer_->IsInitialized())
   {
      return;
   }

   auto& entry = entryIt->second;

   const scwx::qt::render::GeoUniforms uniforms =
      scwx::qt::render::BuildGeoUniforms(
         params, p->thresholded_, p->selectedTime_, resources.layerOpacity);

   const bool uploadGeometry = !entry.geometryUploaded_;

   entry.renderer_->Render(
      commandBuffer,
      uniforms,
      p->currentLinesBuffer_,
      p->currentIntegerBuffer_,
      static_cast<std::uint32_t>(p->currentLineList_.size() *
                                 kVerticesPerRectangle),
      uploadGeometry,
      resources.resourceBatch,
      resources.phase);

   if (uploadGeometry)
   {
      entry.geometryUploaded_ = true;
   }
}

void GeoLines::Deinitialize()
{
   std::unique_lock lock {p->lineMutex_};

   // Only the GPU resources are released here. The CPU-side line list, vertex
   // buffers and hover lines are deliberately kept: they are not GPU objects,
   // and GeoLines can be shared between layer instances (for example the alert
   // layers, see AlertLayerHandler::SharedGeoLines), where a layer that
   // re-initializes (radar site change, pane rebuild) does not repopulate lines
   // that were already populated. Clearing them here left currentLineList_
   // populated but currentLinesBuffer_ empty, so RenderVulkan() returned early
   // and the alert polygons never drew again (and could no longer be hovered).
   // The next RenderVulkan() recreates the renderer and re-uploads geometry.

   for (auto& [rhi, entry] : p->geoRendererByRhi_)
   {
      (void) rhi;
      if (entry.renderer_ != nullptr)
      {
         entry.renderer_->Shutdown();
      }
   }
   p->geoRendererByRhi_.clear();
}

void GeoLines::SetVisible(bool visible)
{
   p->visible_ = visible;
}

void GeoLines::StartLines()
{
   // Clear the new buffers
   p->newLineList_.clear();
   p->newLinesBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverLines_.clear();
}

std::shared_ptr<GeoLineDrawItem> GeoLines::AddLine()
{
   auto& di = p->newLineList_.emplace_back(std::make_shared<GeoLineDrawItem>());
   di->lineIndex_ = p->newLineList_.size() - 1;
   return di;
}

void GeoLines::SetLineLocation(const std::shared_ptr<GeoLineDrawItem>& di,
                               float latitude1,
                               float longitude1,
                               float latitude2,
                               float longitude2)
{
   if (di->latitude1_ != latitude1 || di->longitude1_ != longitude1 ||
       di->latitude2_ != latitude2 || di->longitude2_ != longitude2)
   {
      di->latitude1_  = latitude1;
      di->longitude1_ = longitude1;
      di->latitude2_  = latitude2;
      di->longitude2_ = longitude2;
      p->dirtyLines_.insert(di);
      p->joinsDirty_ = true;
   }
}

void GeoLines::SetLineModulate(const std::shared_ptr<GeoLineDrawItem>& di,
                               boost::gil::rgba8_pixel_t               modulate)
{
   boost::gil::rgba32f_pixel_t newModulate = {modulate[0] / 255.0f,
                                              modulate[1] / 255.0f,
                                              modulate[2] / 255.0f,
                                              modulate[3] / 255.0f};

   if (di->modulate_ != newModulate)
   {
      di->modulate_ = newModulate;
      p->dirtyLines_.insert(di);
      p->joinsDirty_ = true;
   }
}

void GeoLines::SetLineModulate(const std::shared_ptr<GeoLineDrawItem>& di,
                               boost::gil::rgba32f_pixel_t             modulate)
{
   if (di->modulate_ != modulate)
   {
      di->modulate_ = modulate;
      p->dirtyLines_.insert(di);
      p->joinsDirty_ = true;
   }
}

void GeoLines::SetLineWidth(const std::shared_ptr<GeoLineDrawItem>& di,
                            float                                   width)
{
   if (di->width_ != width || di->strokeEnabled_)
   {
      di->width_               = width;
      di->strokeEnabled_       = false;
      di->strokeLineHalf_      = 0.0f;
      di->strokeHighlightHalf_ = 0.0f;
      di->strokeBorderHalf_    = 0.0f;
      p->dirtyLines_.insert(di);
      p->joinsDirty_ = true;
   }
}

void GeoLines::SetLineVisible(const std::shared_ptr<GeoLineDrawItem>& di,
                              bool                                    visible)
{
   if (di->visible_ != visible)
   {
      di->visible_ = visible;
      p->dirtyLines_.insert(di);
   }
}

void GeoLines::SetLineHoverCallback(const std::shared_ptr<GeoLineDrawItem>& di,
                                    const HoverCallback& callback)
{
   if (di->hoverCallback_ != nullptr || callback != nullptr)
   {
      di->hoverCallback_ = callback;
      p->dirtyLines_.insert(di);
   }
}

void GeoLines::SetLineHoverText(const std::shared_ptr<GeoLineDrawItem>& di,
                                const std::string&                      text)
{
   if (di->hoverText_ != text)
   {
      di->hoverText_ = text;
      p->dirtyLines_.insert(di);
   }
}

void GeoLines::SetLineStartTime(const std::shared_ptr<GeoLineDrawItem>& di,
                                std::chrono::system_clock::time_point startTime)
{
   if (di->startTime_ != startTime)
   {
      di->startTime_ =
         std::chrono::time_point_cast<std::chrono::seconds,
                                      std::chrono::system_clock>(startTime);
      p->dirtyLines_.insert(di);
   }
}

void GeoLines::SetLineEndTime(const std::shared_ptr<GeoLineDrawItem>& di,
                              std::chrono::system_clock::time_point   endTime)
{
   if (di->endTime_ != endTime)
   {
      di->endTime_ =
         std::chrono::time_point_cast<std::chrono::seconds,
                                      std::chrono::system_clock>(endTime);
      p->dirtyLines_.insert(di);
   }
}

void GeoLines::SetLineStrokeStyle(
   const std::shared_ptr<GeoLineDrawItem>& di,
   const boost::gil::rgba32f_pixel_t&      lineColor,
   const boost::gil::rgba32f_pixel_t&      highlightColor,
   const boost::gil::rgba32f_pixel_t&      borderColor,
   float                                   lineHalf,
   float                                   highlightHalf,
   float                                   borderHalf)
{
   const float outerWidth = borderHalf * 2.0f;
   if (di->modulate_ != lineColor || di->highlightColor_ != highlightColor ||
       di->borderColor_ != borderColor || !di->strokeEnabled_ ||
       di->strokeLineHalf_ != lineHalf ||
       di->strokeHighlightHalf_ != highlightHalf ||
       di->strokeBorderHalf_ != borderHalf || di->width_ != outerWidth)
   {
      di->modulate_            = lineColor;
      di->highlightColor_      = highlightColor;
      di->borderColor_         = borderColor;
      di->strokeEnabled_       = true;
      di->strokeLineHalf_      = lineHalf;
      di->strokeHighlightHalf_ = highlightHalf;
      di->strokeBorderHalf_    = borderHalf;
      di->width_               = outerWidth;
      p->dirtyLines_.insert(di);
      p->joinsDirty_ = true;
   }
}

void GeoLines::FinishLines()
{
   std::unique_lock lock {p->lineMutex_};

   // Update buffers
   p->UpdateBuffers();

   // Swap buffers
   p->currentLineList_ = p->newLineList_;
   p->currentLinesBuffer_.swap(p->newLinesBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);
   p->currentHoverLines_.swap(p->newHoverLines_);

   // Clear the new buffers, except the full line list (used to update buffers
   // without re-adding lines)
   p->newLinesBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverLines_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
   p->InvalidateGeometryUploads();
}

void GeoLines::Impl::UpdateBuffers()
{
   ResolveJoins();
   joinsDirty_ = false;

   newLinesBuffer_.clear();
   newLinesBuffer_.reserve(newLineList_.size() * kLineBufferLength_);
   newIntegerBuffer_.clear();
   newIntegerBuffer_.reserve(newLineList_.size() * kVerticesPerRectangle *
                             kIntegersPerVertex_);
   newHoverLines_.clear();

   for (std::size_t i = 0; i < newLineList_.size(); ++i)
   {
      auto& di = newLineList_[i];

      // Update line buffer
      UpdateSingleBuffer(
         di, newLinesBuffer_, newIntegerBuffer_, newHoverLines_);
   }

   // All lines have been updated
   dirtyLines_.clear();
}

void GeoLines::Impl::UpdateModifiedLineBuffers()
{
   // Synchronize line list
   currentLineList_ = newLineList_;
   currentLinesBuffer_.resize(currentLineList_.size() * kLineBufferLength_);
   currentIntegerBuffer_.resize(currentLineList_.size() *
                                kVerticesPerRectangle * kIntegersPerVertex_);

   // Lines whose endpoint joins changed need new vertices, as do the lines
   // joined to any modified line (a corner depends on both of its segments)
   boost::unordered_flat_set<std::shared_ptr<GeoLineDrawItem>> updateLines {};

   if (joinsDirty_)
   {
      for (const std::size_t index : ResolveJoins())
      {
         updateLines.insert(currentLineList_[index]);
      }
      joinsDirty_ = false;
   }

   for (auto& di : dirtyLines_)
   {
      updateLines.insert(di);

      for (const auto* join : {&di->joinStart_, &di->joinEnd_})
      {
         if (join->kind_ == GeoLineDrawItem::EndJoin::Kind::Joined &&
             join->other_ < currentLineList_.size())
         {
            updateLines.insert(currentLineList_[join->other_]);
         }
      }
   }

   // Update buffers for modified lines
   for (auto& di : updateLines)
   {
      // Check if modified line is in the current list
      if (di->lineIndex_ >= currentLineList_.size() ||
          currentLineList_[di->lineIndex_] != di)
      {
         continue;
      }

      UpdateSingleBuffer(
         di, currentLinesBuffer_, currentIntegerBuffer_, currentHoverLines_);
   }

   // Clear list of modified lines
   if (!dirtyLines_.empty() || !updateLines.empty())
   {
      dirtyLines_.clear();
      dirty_ = true;
      InvalidateGeometryUploads();
   }
}

namespace
{

static constexpr float kHoverPickExtraHalfPx = 18.0f;
static constexpr float kLineCapOverlapPx     = 1.0f;

static bool
IsGeoLinePickable(const GeoLineDrawItem&                       drawItem,
                  const units::length::meters<double>&         mapDistance,
                  const std::chrono::system_clock::time_point& selectedTime)
{
   if ((mapDistance > units::length::meters<double> {0.0} &&
        static_cast<int>(std::round(
           units::length::nautical_miles<double> {drawItem.threshold_}
              .value())) < 999 &&
        drawItem.threshold_ < mapDistance &&
        (drawItem.threshold_.value() >= 0.0 ||
         -(drawItem.threshold_) > mapDistance)) ||
       (drawItem.startTime_ != std::chrono::system_clock::time_point {} &&
        (selectedTime < drawItem.startTime_ ||
         drawItem.endTime_ <= selectedTime)))
   {
      return false;
   }

   return true;
}

static float PickHalfWidthMercator(const glm::mat4& mapMatrix,
                                   const float      pickHalfPx)
{
   const glm::vec2 xAxis =
      glm::vec2(mapMatrix * glm::vec4 {pickHalfPx, 0.0f, 0.0f, 1.0f});
   const glm::vec2 yAxis =
      glm::vec2(mapMatrix * glm::vec4 {0.0f, pickHalfPx, 0.0f, 1.0f});
   return std::max(glm::length(xAxis), glm::length(yAxis));
}

static bool IsPointNearGeoLine(const GeoLineHoverEntry& line,
                               const glm::mat4&         mapMatrix,
                               const glm::vec2&         mouseCoords)
{
   const float hw     = line.di_->strokeEnabled_ ? line.di_->strokeBorderHalf_ :
                                                   (line.di_->width_ * 0.5f);
   const float pickHw = hw + kHoverPickExtraHalfPx;

   glm::vec2 bl = line.p1_;
   glm::vec2 br = bl;
   glm::vec2 tl = line.p2_;
   glm::vec2 tr = tl;

   const glm::vec2 otl = mapMatrix * glm::vec4 {line.otl_, 0.0f, 1.0f};
   const glm::vec2 obl = mapMatrix * glm::vec4 {line.obl_, 0.0f, 1.0f};
   const glm::vec2 obr = mapMatrix * glm::vec4 {line.obr_, 0.0f, 1.0f};
   const glm::vec2 otr = mapMatrix * glm::vec4 {line.otr_, 0.0f, 1.0f};

   tl += otl;
   bl += obl;
   br += obr;
   tr += otr;

   if (util::maplibre::IsPointInPolygon({tl, bl, br, tr}, mouseCoords))
   {
      return true;
   }

   const glm::vec2 ab    = line.p2_ - line.p1_;
   const float     lenSq = glm::dot(ab, ab);
   if (lenSq <= 1e-10f)
   {
      return false;
   }

   const float t =
      glm::clamp(glm::dot(mouseCoords - line.p1_, ab) / lenSq, 0.0f, 1.0f);
   const float dist = glm::length(mouseCoords - (line.p1_ + t * ab));
   return dist <= PickHalfWidthMercator(mapMatrix, pickHw);
}

// Longest miter, in line half-widths, before the corner is clamped
static constexpr float kMiterLimit = 3.0f;

static float StrokeHalfWidth(const GeoLineDrawItem& di)
{
   return di.strokeEnabled_ ? di.strokeBorderHalf_ : (di.width_ * 0.5f);
}

static std::uint64_t EndpointKey(const float latitude, const float longitude)
{
   std::uint32_t latBits {};
   std::uint32_t lonBits {};
   std::memcpy(&latBits, &latitude, sizeof(latBits));
   std::memcpy(&lonBits, &longitude, sizeof(lonBits));
   return (static_cast<std::uint64_t>(latBits) << 32) | lonBits;
}

static bool IsDegenerate(const GeoLineDrawItem& di)
{
   return di.latitude1_ == di.latitude2_ && di.longitude1_ == di.longitude2_;
}

// Two lines can share a corner only if they are drawn identically
static bool CanJoin(const GeoLineDrawItem& a, const GeoLineDrawItem& b)
{
   constexpr float kEpsilon = 1.0e-4f;

   if (a.strokeEnabled_ != b.strokeEnabled_ || a.modulate_ != b.modulate_ ||
       std::abs(StrokeHalfWidth(a) - StrokeHalfWidth(b)) > kEpsilon)
   {
      return false;
   }

   return !a.strokeEnabled_ ||
          (a.highlightColor_ == b.highlightColor_ &&
           a.borderColor_ == b.borderColor_ &&
           std::abs(a.strokeLineHalf_ - b.strokeLineHalf_) <= kEpsilon &&
           std::abs(a.strokeHighlightHalf_ - b.strokeHighlightHalf_) <=
              kEpsilon);
}

// Unit direction of a line from its first to its second endpoint, in the same
// (Mercator, north up) space the vertex shader projects to
static glm::vec2 LineDirection(const GeoLineDrawItem& di)
{
   const glm::vec2 sc1 = util::maplibre::LatLongToScreenCoordinate(
      {di.latitude1_, di.longitude1_});
   const glm::vec2 sc2 = util::maplibre::LatLongToScreenCoordinate(
      {di.latitude2_, di.longitude2_});
   glm::vec2   along {sc2.x - sc1.x, sc2.y - sc1.y};
   const float alongLen = glm::length(along);
   if (alongLen > 1.0e-6f)
   {
      return along / alongLen;
   }
   return glm::vec2 {0.0f, 1.0f};
}

static glm::vec2 LeftNormal(const glm::vec2& along)
{
   return {-along.y, along.x};
}

} // namespace

std::vector<std::size_t> GeoLines::Impl::ResolveJoins()
{
   using EndJoin = GeoLineDrawItem::EndJoin;
   using Kind    = EndJoin::Kind;

   struct EndRef
   {
      std::size_t line_;
      bool        atEnd_;
   };

   std::unordered_map<std::uint64_t, std::vector<EndRef>> endpoints {};
   endpoints.reserve(newLineList_.size() * 2);

   for (std::size_t i = 0; i < newLineList_.size(); ++i)
   {
      const GeoLineDrawItem& di = *newLineList_[i];
      if (IsDegenerate(di))
      {
         continue;
      }
      endpoints[EndpointKey(di.latitude1_, di.longitude1_)].push_back(
         {i, false});
      endpoints[EndpointKey(di.latitude2_, di.longitude2_)].push_back(
         {i, true});
   }

   std::vector<EndJoin> starts(newLineList_.size());
   std::vector<EndJoin> ends(newLineList_.size());

   const auto setJoin = [&](const EndRef& self, const EndJoin& join)
   {
      (self.atEnd_ ? ends : starts)[self.line_] = join;
   };

   for (const auto& [key, refs] : endpoints)
   {
      (void) key;
      if (refs.size() == 1)
      {
         continue; // Open
      }

      // Identical segments (same two endpoints, any direction) are routinely
      // submitted more than once, e.g. an alert polygon shared by several
      // alerts. They draw on top of each other, so they count as one neighbor
      // when deciding whether a corner is a plain two-line join.
      struct Group
      {
         std::uint64_t       otherEnd_;
         std::vector<EndRef> members_ {};
      };
      std::vector<Group> groups {};
      for (const EndRef& ref : refs)
      {
         const GeoLineDrawItem& di = *newLineList_[ref.line_];
         const std::uint64_t    otherEnd =
            ref.atEnd_ ? EndpointKey(di.latitude1_, di.longitude1_) :
                         EndpointKey(di.latitude2_, di.longitude2_);

         auto it = std::find_if(groups.begin(),
                                groups.end(),
                                [&](const Group& g)
                                { return g.otherEnd_ == otherEnd; });
         if (it == groups.end())
         {
            groups.push_back({otherEnd, {ref}});
         }
         else
         {
            it->members_.push_back(ref);
         }
      }

      bool joinable = groups.size() == 2;
      if (joinable)
      {
         const GeoLineDrawItem& repA =
            *newLineList_[groups[0].members_.front().line_];
         const GeoLineDrawItem& repB =
            *newLineList_[groups[1].members_.front().line_];
         joinable = CanJoin(repA, repB);

         for (std::size_t g = 0; joinable && g < 2; ++g)
         {
            const GeoLineDrawItem& rep =
               *newLineList_[groups[g].members_.front().line_];
            for (const EndRef& member : groups[g].members_)
            {
               joinable = joinable && CanJoin(rep, *newLineList_[member.line_]);
            }
         }
      }

      if (joinable)
      {
         const EndRef& partnerA = groups[1].members_.front();
         const EndRef& partnerB = groups[0].members_.front();
         for (const EndRef& member : groups[0].members_)
         {
            setJoin(member, {Kind::Joined, partnerA.line_, partnerA.atEnd_});
         }
         for (const EndRef& member : groups[1].members_)
         {
            setJoin(member, {Kind::Joined, partnerB.line_, partnerB.atEnd_});
         }
         continue;
      }

      for (const EndRef& ref : refs)
      {
         setJoin(ref, {Kind::Blocked, 0, false});
      }
   }

   std::vector<std::size_t> changed {};
   for (std::size_t i = 0; i < newLineList_.size(); ++i)
   {
      GeoLineDrawItem& di = *newLineList_[i];
      if (di.joinStart_ != starts[i] || di.joinEnd_ != ends[i])
      {
         di.joinStart_ = starts[i];
         di.joinEnd_   = ends[i];
         changed.push_back(i);
      }
   }

   return changed;
}

void GeoLines::Impl::UpdateSingleBuffer(
   const std::shared_ptr<GeoLineDrawItem>& di,
   std::vector<float>&                     lineBuffer,
   std::vector<std::int32_t>&              integerBuffer,
   std::unordered_map<std::shared_ptr<GeoLineDrawItem>, GeoLineHoverEntry>&
      hoverLines)
{
   // Threshold value
   units::length::nautical_miles<double> threshold = di->threshold_;
   auto                                  thresholdValue =
      static_cast<std::int32_t>(std::round(threshold.value()));

   // Start and end time
   auto startTime = static_cast<std::int32_t>(
      std::chrono::duration_cast<std::chrono::minutes>(
         di->startTime_.time_since_epoch())
         .count());
   auto endTime = static_cast<std::int32_t>(
      std::chrono::duration_cast<std::chrono::minutes>(
         di->endTime_.time_since_epoch())
         .count());

   // Latitude and longitude coordinates in degrees
   const float lat1 = di->latitude1_;
   const float lon1 = di->longitude1_;
   const float lat2 = di->latitude2_;
   const float lon2 = di->longitude2_;

   // TODO: Base X/Y offsets in pixels
   // const float x1 = static_cast<float>(di->x1_);
   // const float y1 = static_cast<float>(di->y1_);
   // const float x2 = static_cast<float>(di->x2_);
   // const float y2 = static_cast<float>(di->y2_);

   // Offset pixels use OrthoMapProjection (Y-down). CPU mercator Y already
   // increases north. along = (dx, dy).
   const units::angle::degrees<double> angle =
      util::GeographicLib::GetAngle(lat1, lon1, lat2, lon2);

   const glm::vec2 sc1 =
      util::maplibre::LatLongToScreenCoordinate({lat1, lon1});
   const glm::vec2 sc2 =
      util::maplibre::LatLongToScreenCoordinate({lat2, lon2});
   const glm::vec2 along = LineDirection(*di);
   const glm::vec2 perp  = LeftNormal(along);

   // Final X/Y offsets in pixels
   const float hw =
      di->strokeEnabled_ ? di->strokeBorderHalf_ : (di->width_ * 0.5f);
   const float pickHw = hw + kHoverPickExtraHalfPx;
   const float lx     = -hw;
   const float rx     = +hw;

   // Offset of the left (side -1) and right (side +1) vertex at one endpoint.
   //
   // Where exactly one compatible line continues from the endpoint (polygon
   // outlines, polylines), both lines use the same mitered corner, so their
   // quads tile exactly with no gap on the outside of the turn and no overlap
   // on the inside. The perpendicular offset attribute stays +-hw, so the
   // line / highlight / border bands follow the corner.
   //
   // An endpoint nothing else ends at is a plain butt end. If other lines end
   // there but cannot be joined (a hairpin turn, different styles, three or
   // more lines), the end is extended by a square cap to cover the gap.
   const auto endOffsets =
      [&](const GeoLineDrawItem::EndJoin& join, const bool atEnd)
   {
      using Kind = GeoLineDrawItem::EndJoin::Kind;

      const float     direction = atEnd ? 1.0f : -1.0f;
      const glm::vec2 left      = perp * lx;
      const glm::vec2 right     = perp * rx;

      if (join.kind_ == Kind::Joined && join.other_ < newLineList_.size())
      {
         const glm::vec2 otherNormal =
            LeftNormal(LineDirection(*newLineList_[join.other_]));

         // Walking from this line through the corner to the other line, the
         // other line's left is the same side only if it continues the same
         // way, i.e. this end is the end of one line and the start of the other
         const float     sameWay = (join.otherAtEnd_ == atEnd) ? -1.0f : 1.0f;
         const glm::vec2 n2      = otherNormal * sameWay;
         const float     denom   = 1.0f + glm::dot(perp, n2);

         if (denom > 1.0e-3f)
         {
            // Offset of the right side; the left side is its mirror image
            glm::vec2 miter = (perp + n2) * (hw / denom);

            const float maxLength = kMiterLimit * hw;
            const float length    = glm::length(miter);
            if (length > maxLength && length > 0.0f)
            {
               miter *= maxLength / length;
            }

            return std::pair {-miter, miter};
         }
      }

      const float cap =
         (join.kind_ == Kind::Open) ? 0.0f : (hw + kLineCapOverlapPx);
      return std::pair {left + along * (direction * cap),
                        right + along * (direction * cap)};
   };

   const auto [bl, br] = endOffsets(di->joinStart_, false);
   const auto [tl, tr] = endOffsets(di->joinEnd_, true);
   const float blX     = bl.x;
   const float blY     = bl.y;
   const float tlX     = tl.x;
   const float tlY     = tl.y;
   const float brX     = br.x;
   const float brY     = br.y;
   const float trX     = tr.x;
   const float trY     = tr.y;

   // Modulate color
   const float mc0 = di->modulate_[0];
   const float mc1 = di->modulate_[1];
   const float mc2 = di->modulate_[2];
   const float mc3 = di->modulate_[3];

   const float hc0 = di->highlightColor_[0];
   const float hc1 = di->highlightColor_[1];
   const float hc2 = di->highlightColor_[2];
   const float hc3 = di->highlightColor_[3];

   const float bc0 = di->borderColor_[0];
   const float bc1 = di->borderColor_[1];
   const float bc2 = di->borderColor_[2];
   const float bc3 = di->borderColor_[3];

   const float sh0 = di->strokeEnabled_ ? di->strokeLineHalf_ : 0.0f;
   const float sh1 = di->strokeEnabled_ ? di->strokeHighlightHalf_ : 0.0f;
   const float sh2 = di->strokeEnabled_ ? di->strokeBorderHalf_ : 0.0f;

   // Visibility
   const auto v = static_cast<std::int32_t>(di->visible_);

   // Initiailize line data (aAngleDeg slot = unrotated perpendicular offset)
   const auto lineData = {
      // Line
      lat1, lon1, blX, blY, mc0, mc1, mc2, mc3, lx,  hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // BL
      lat2, lon2, tlX, tlY, mc0, mc1, mc2, mc3, lx,  hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // TL
      lat1, lon1, brX, brY, mc0, mc1, mc2, mc3, rx,  hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // BR
      lat1, lon1, brX, brY, mc0, mc1, mc2, mc3, rx,  hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // BR
      lat2, lon2, trX, trY, mc0, mc1, mc2, mc3, rx,  hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // TR
      lat2, lon2, tlX, tlY, mc0, mc1, mc2, mc3, lx,  hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2 // TL
   };
   const auto integerData = {thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v};

   // Buffer position data
   auto lineBufferPosition = lineBuffer.end();
   auto lineBufferOffset   = di->lineIndex_ * kLineBufferLength_;

   auto integerBufferPosition = integerBuffer.end();
   auto integerBufferOffset   = di->lineIndex_ * kIntegerBufferLength_;

   if (lineBufferOffset < lineBuffer.size())
   {
      lineBufferPosition = lineBuffer.begin() + lineBufferOffset;
   }
   if (integerBufferOffset < integerBuffer.size())
   {
      integerBufferPosition = integerBuffer.begin() + integerBufferOffset;
   }

   if (lineBufferPosition == lineBuffer.cend())
   {
      lineBuffer.insert(lineBufferPosition, lineData);
   }
   else
   {
      std::copy(lineData.begin(), lineData.end(), lineBufferPosition);
   }

   if (integerBufferPosition == integerBuffer.cend())
   {
      integerBuffer.insert(integerBufferPosition, integerData);
   }
   else
   {
      std::copy(integerData.begin(), integerData.end(), integerBufferPosition);
   }

   auto hoverIt = hoverLines.find(di);

   if (di->visible_ && (!di->hoverText_.empty() ||
                        di->hoverCallback_ != nullptr || di->event_ != nullptr))
   {
      const units::angle::radians<double> radians = angle;

      // Reuse outer sc1/sc2 (MSVC C4456 if redeclared here).
      const float cosAngle = cosf(static_cast<float>(radians.value()));
      const float sinAngle = sinf(static_cast<float>(radians.value()));

      const glm::mat2 rotate {cosAngle, -sinAngle, sinAngle, cosAngle};

      const glm::vec2 otl = rotate * glm::vec2 {-pickHw, +pickHw};
      const glm::vec2 otr = rotate * glm::vec2 {+pickHw, +pickHw};
      const glm::vec2 obl = rotate * glm::vec2 {-pickHw, -pickHw};
      const glm::vec2 obr = rotate * glm::vec2 {+pickHw, -pickHw};

      if (hoverIt == hoverLines.end())
      {
         hoverLines.emplace(di,
                            GeoLineHoverEntry {.di_  = di,
                                               .p1_  = sc1,
                                               .p2_  = sc2,
                                               .otl_ = otl,
                                               .otr_ = otr,
                                               .obl_ = obl,
                                               .obr_ = obr});
      }
      else
      {
         hoverIt->second.p1_  = sc1;
         hoverIt->second.p2_  = sc2;
         hoverIt->second.otl_ = otl;
         hoverIt->second.otr_ = otr;
         hoverIt->second.obl_ = obl;
         hoverIt->second.obr_ = obr;
      }
   }
   else if (hoverIt != hoverLines.end())
   {
      hoverLines.erase(hoverIt);
   }
}

void GeoLines::Impl::Update()
{
   UpdateModifiedLineBuffers();

   dirty_ = false;
}

bool GeoLines::RunMousePicking(
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF& /* mouseLocalPos */,
   const QPointF&   mouseGlobalPos,
   const glm::vec2& mouseCoords,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& eventHandler)
{
   std::unique_lock lock {p->lineMutex_};

   bool itemPicked = false;

   // Calculate map scale, remove width and height from original calculation
   glm::vec2 scale = util::maplibre::GetMapScale(params);
   scale = 2.0f / glm::vec2 {scale.x * params.width, scale.y * params.height};

   // Scale and rotate the identity matrix to create the map matrix
   glm::mat4 mapMatrix {1.0f};
   mapMatrix = glm::scale(mapMatrix, glm::vec3 {scale, 1.0f});
   mapMatrix = glm::rotate(mapMatrix,
                           glm::radians<float>(params.bearing),
                           glm::vec3(0.0f, 0.0f, 1.0f));

   units::length::meters<double> mapDistance =
      (p->thresholded_) ? util::maplibre::GetMapDistance(params) :
                          units::length::meters<double> {0.0};

   // If no time has been selected, use the current time
   std::chrono::system_clock::time_point selectedTime =
      (p->selectedTime_ == std::chrono::system_clock::time_point {}) ?
         scwx::util::time::now() :
         p->selectedTime_;

   // For each pickable line (top-most first)
   std::shared_ptr<GeoLineDrawItem> pickedDrawItem;
   std::string                      hoverText;
   HoverCallback                    hoverCallback;

   for (auto lineIt = p->currentLineList_.crbegin();
        lineIt != p->currentLineList_.crend();
        ++lineIt)
   {
      auto hoverIt = p->currentHoverLines_.find(*lineIt);
      if (hoverIt == p->currentHoverLines_.cend())
      {
         continue;
      }

      const auto& line = hoverIt->second;
      if (!IsGeoLinePickable(*line.di_, mapDistance, selectedTime))
      {
         continue;
      }

      if (!IsPointNearGeoLine(line, mapMatrix, mouseCoords))
      {
         continue;
      }

      pickedDrawItem = line.di_;
      hoverText      = line.di_->hoverText_;
      hoverCallback  = line.di_->hoverCallback_;
      break;
   }

   if (pickedDrawItem != nullptr)
   {
      itemPicked = true;

      lock.unlock();

      if (!hoverText.empty())
      {
         // Show tooltip
         util::tooltip::Show(hoverText, mouseGlobalPos);
      }
      else if (hoverCallback != nullptr)
      {
         hoverCallback(pickedDrawItem, mouseGlobalPos);
      }

      if (pickedDrawItem->event_ != nullptr)
      {
         // Register event handler
         eventHandler = pickedDrawItem;
      }
   }

   return itemPicked;
}

void GeoLines::RegisterEventHandler(
   const std::shared_ptr<GeoLineDrawItem>& di,
   const std::function<void(QEvent*)>&     eventHandler)
{
   di->event_ = eventHandler;
}

} // namespace draw
} // namespace qt
} // namespace scwx
