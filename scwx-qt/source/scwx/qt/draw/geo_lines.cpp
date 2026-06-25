#include <scwx/qt/draw/geo_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <memory>

#include <boost/unordered/unordered_flat_set.hpp>
#include <units/angle.h>

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
   void UpdateSingleBuffer(const std::shared_ptr<GeoLineDrawItem>& di,
                           std::vector<float>&                     linesBuffer,
                           std::vector<std::int32_t>&             integerBuffer,
                           std::unordered_map<std::shared_ptr<GeoLineDrawItem>,
                                              GeoLineHoverEntry>& hoverLines);

   std::shared_ptr<render::RenderContext> context_;

   bool visible_ {true};
   bool dirty_ {false};
   bool thresholded_ {false};

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

   std::unique_ptr<render::RhiGeoColoredGeometry> geoRenderer_ {};
   bool                                           geometryUploaded_ {false};
   std::uint64_t                                  renderTargetGeneration_ {0};

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

void GeoLines::Impl::EnsureGeoRenderer(
   render::RhiVulkanOverlayResources& resources,
   QRhiCommandBuffer*                 commandBuffer)
{
   if (resources.rhi == nullptr || resources.renderTarget == nullptr)
   {
      return;
   }

   if (renderTargetGeneration_ != resources.renderTargetGeneration)
   {
      if (geoRenderer_ != nullptr)
      {
         geoRenderer_->Shutdown();
      }
      geoRenderer_ = std::make_unique<render::RhiGeoColoredGeometry>();
      renderTargetGeneration_ = resources.renderTargetGeneration;
      geometryUploaded_       = false;
   }

   if (geoRenderer_ == nullptr)
   {
      geoRenderer_ = std::make_unique<render::RhiGeoColoredGeometry>();
      renderTargetGeneration_ = resources.renderTargetGeneration;
   }

   if (!geoRenderer_->IsInitialized())
   {
      geoRenderer_->Initialize(
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
   if (p->geoRenderer_ == nullptr || !p->geoRenderer_->IsInitialized())
   {
      return;
   }

   const scwx::qt::render::GeoUniforms uniforms =
      scwx::qt::render::BuildGeoUniforms(
         params, p->thresholded_, p->selectedTime_);

   const bool uploadGeometry = !p->geometryUploaded_;

   p->geoRenderer_->Render(
      commandBuffer,
      uniforms,
      p->currentLinesBuffer_,
      p->currentIntegerBuffer_,
      static_cast<std::uint32_t>(p->currentLineList_.size() *
                                 kVerticesPerRectangle),
      uploadGeometry);

   if (uploadGeometry)
   {
      p->geometryUploaded_ = true;
   }
}

void GeoLines::Deinitialize()
{
   std::unique_lock lock {p->lineMutex_};

   p->currentLinesBuffer_.clear();
   p->currentIntegerBuffer_.clear();
   p->currentHoverLines_.clear();

   if (p->geoRenderer_ != nullptr)
   {
      p->geoRenderer_->Shutdown();
      p->geoRenderer_.reset();
   }
   p->geometryUploaded_       = false;
   p->renderTargetGeneration_ = 0;
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
   }
}

void GeoLines::SetLineModulate(const std::shared_ptr<GeoLineDrawItem>& di,
                               boost::gil::rgba32f_pixel_t             modulate)
{
   if (di->modulate_ != modulate)
   {
      di->modulate_ = modulate;
      p->dirtyLines_.insert(di);
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
   p->dirty_            = true;
   p->geometryUploaded_ = false;
}

void GeoLines::Impl::UpdateBuffers()
{
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

   // Update buffers for modified lines
   for (auto& di : dirtyLines_)
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
   if (!dirtyLines_.empty())
   {
      dirtyLines_.clear();
      dirty_            = true;
      geometryUploaded_ = false;
   }
}

namespace
{

static constexpr float kHoverPickExtraHalfPx = 18.0f;

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

} // namespace

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

   // Angle
   const units::angle::degrees<double> angle =
      util::GeographicLib::GetAngle(lat1, lon1, lat2, lon2);
   const float a = static_cast<float>(angle.value());

   // Final X/Y offsets in pixels
   const float hw =
      di->strokeEnabled_ ? di->strokeBorderHalf_ : (di->width_ * 0.5f);
   const float pickHw = hw + kHoverPickExtraHalfPx;
   const float lx     = -hw;
   const float rx     = +hw;
   const float ty     = +hw;
   const float by     = -hw;

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

   // Initiailize line data
   const auto lineData = {
      // Line
      lat1, lon1, lx,  by,  mc0, mc1, mc2, mc3, a,   hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // BL
      lat2, lon2, lx,  ty,  mc0, mc1, mc2, mc3, a,   hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // TL
      lat1, lon1, rx,  by,  mc0, mc1, mc2, mc3, a,   hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // BR
      lat1, lon1, rx,  by,  mc0, mc1, mc2, mc3, a,   hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // BR
      lat2, lon2, rx,  ty,  mc0, mc1, mc2, mc3, a,   hc0,
      hc1,  hc2,  hc3, bc0, bc1, bc2, bc3, sh0, sh1, sh2, // TR
      lat2, lon2, lx,  ty,  mc0, mc1, mc2, mc3, a,   hc0,
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

      const auto sc1 = util::maplibre::LatLongToScreenCoordinate({lat1, lon1});
      const auto sc2 = util::maplibre::LatLongToScreenCoordinate({lat2, lon2});

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
