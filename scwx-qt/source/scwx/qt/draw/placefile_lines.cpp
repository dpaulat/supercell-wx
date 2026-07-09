#include <scwx/qt/draw/placefile_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <scwx/qt/render/rhi_geo_colored_geometry.hpp>
#include <scwx/qt/render/rhi_geo_uniforms.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <array>
#include <execution>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include <rhi/qrhi.h>

namespace scwx
{
namespace qt
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::draw::placefile_lines";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kVerticesPerTriangle  = 3;
static constexpr std::size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;

// Threshold, start time, end time
static constexpr std::size_t kIntegersPerVertex_ = 3;

static constexpr std::array<float, 11> kNoStrokeVertexPadding = {
   0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

static const boost::gil::rgba8_pixel_t kBlack_ {0, 0, 0, 255};

class PlacefileLines::Impl
{
public:
   struct LineHoverEntry
   {
      std::shared_ptr<const gr::Placefile::LineDrawItem> di_;

      glm::vec2 p1_;
      glm::vec2 p2_;
      glm::vec2 otl_;
      glm::vec2 otr_;
      glm::vec2 obl_;
      glm::vec2 obr_;
   };

   explicit Impl(const std::shared_ptr<render::RenderContext>& context) :
       context_ {context}, numVertices_ {0}
   {
   }

   ~Impl() {}

   void BufferLine(const std::shared_ptr<const gr::Placefile::LineDrawItem>& di,
                   const gr::Placefile::LineDrawItem::Element&               e1,
                   const gr::Placefile::LineDrawItem::Element&               e2,
                   const float                         width,
                   const units::angle::degrees<double> angle,
                   const boost::gil::rgba8_pixel_t     color,
                   const std::int32_t                  threshold,
                   const std::int32_t                  startTime,
                   const std::int32_t                  endTime,
                   bool                                bufferHover = false);
   void
   UpdateBuffers(const std::shared_ptr<const gr::Placefile::LineDrawItem>& di);
   void Update();

   std::shared_ptr<render::RenderContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex lineMutex_ {};

   std::size_t currentNumLines_ {};
   std::size_t newNumLines_ {};

   std::vector<float>        currentLinesBuffer_ {};
   std::vector<std::int32_t> currentIntegerBuffer_ {};
   std::vector<float>        newLinesBuffer_ {};
   std::vector<std::int32_t> newIntegerBuffer_ {};

   std::vector<LineHoverEntry> currentHoverLines_ {};
   std::vector<LineHoverEntry> newHoverLines_ {};

   std::uint32_t numVertices_;

   std::vector<std::int32_t>                      expandedIntegerBuffer_ {};
   struct GeoRendererCacheEntry
   {
      std::unique_ptr<render::RhiGeoColoredGeometry> renderer_ {};
      bool                                           geometryUploaded_ {false};
      std::uint64_t                                  renderTargetGeneration_ {0};
   };

   std::unordered_map<QRhi*, GeoRendererCacheEntry> geoRendererByRhi_ {};

   void InvalidateGeometryUploads();
   void RebuildExpandedIntegerBuffer();
   void EnsureGeoRenderer(render::RhiVulkanOverlayResources& resources,
                          QRhiCommandBuffer*                 commandBuffer);
};

PlacefileLines::PlacefileLines(
   const std::shared_ptr<render::RenderContext>& context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
PlacefileLines::~PlacefileLines() = default;

PlacefileLines::PlacefileLines(PlacefileLines&&) noexcept            = default;
PlacefileLines& PlacefileLines::operator=(PlacefileLines&&) noexcept = default;

void PlacefileLines::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void PlacefileLines::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileLines::Initialize() {}

void PlacefileLines::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

void PlacefileLines::Impl::RebuildExpandedIntegerBuffer()
{
   expandedIntegerBuffer_.clear();
   expandedIntegerBuffer_.reserve(currentIntegerBuffer_.size() /
                                  kIntegersPerVertex_ * 4);
   for (std::size_t i = 0; i < currentIntegerBuffer_.size();
        i += kIntegersPerVertex_)
   {
      expandedIntegerBuffer_.push_back(currentIntegerBuffer_[i]);
      expandedIntegerBuffer_.push_back(currentIntegerBuffer_[i + 1]);
      expandedIntegerBuffer_.push_back(currentIntegerBuffer_[i + 2]);
      expandedIntegerBuffer_.push_back(1);
   }
   InvalidateGeometryUploads();
}

void PlacefileLines::Impl::InvalidateGeometryUploads()
{
   for (auto& [rhi, entry] : geoRendererByRhi_)
   {
      (void) rhi;
      entry.geometryUploaded_ = false;
   }
}

void PlacefileLines::Impl::EnsureGeoRenderer(
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
      entry.renderer_               = std::make_unique<render::RhiGeoColoredGeometry>();
      entry.renderTargetGeneration_ = resources.renderTargetGeneration;
      entry.geometryUploaded_       = false;
   }

   if (entry.renderer_ == nullptr)
   {
      entry.renderer_               = std::make_unique<render::RhiGeoColoredGeometry>();
      entry.renderTargetGeneration_ = resources.renderTargetGeneration;
   }

   if (!entry.renderer_->IsInitialized())
   {
      entry.renderer_->Initialize(
         resources.rhi, resources.renderTarget, commandBuffer);
   }
}

void PlacefileLines::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool /* textureAtlasChanged */)
{
   std::unique_lock lock {p->lineMutex_};

   if (p->currentNumLines_ == 0 || p->currentLinesBuffer_.empty())
   {
      return;
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
         params, p->thresholded_, p->selectedTime_);

   const bool uploadGeometry = !entry.geometryUploaded_;

   entry.renderer_->Render(commandBuffer,
                           uniforms,
                           p->currentLinesBuffer_,
                           p->expandedIntegerBuffer_,
                           static_cast<std::uint32_t>(p->numVertices_),
                           uploadGeometry,
                           resources.resourceBatch,
                           resources.phase);

   if (uploadGeometry)
   {
      entry.geometryUploaded_ = true;
   }
}

void PlacefileLines::Deinitialize()
{
   std::unique_lock lock {p->lineMutex_};

   p->currentLinesBuffer_.clear();
   p->currentIntegerBuffer_.clear();
   p->currentHoverLines_.clear();

   p->expandedIntegerBuffer_.clear();
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

void PlacefileLines::StartLines()
{
   // Clear the new buffers
   p->newLinesBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverLines_.clear();

   p->newNumLines_ = 0u;
}

void PlacefileLines::AddLine(
   const std::shared_ptr<gr::Placefile::LineDrawItem>& di)
{
   if (di != nullptr && !di->elements_.empty())
   {
      p->UpdateBuffers(di);
      p->newNumLines_ += (di->elements_.size() - 1) * 2;
   }
}

void PlacefileLines::FinishLines()
{
   std::unique_lock lock {p->lineMutex_};

   // Swap buffers
   p->currentLinesBuffer_.swap(p->newLinesBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);
   p->currentHoverLines_.swap(p->newHoverLines_);

   // Clear the new buffers
   p->newLinesBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverLines_.clear();

   // Update the number of lines
   p->currentNumLines_ = p->newNumLines_;
   p->numVertices_ =
      static_cast<std::uint32_t>(p->currentNumLines_ * kVerticesPerRectangle);

   // Mark the draw item dirty
   p->dirty_ = true;
   p->RebuildExpandedIntegerBuffer();
}

void PlacefileLines::Impl::UpdateBuffers(
   const std::shared_ptr<const gr::Placefile::LineDrawItem>& di)
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

   std::vector<units::angle::degrees<double>> angles {};
   angles.reserve(di->elements_.size() - 1);

   // For each element pair inside a Line statement, render a black line
   for (std::size_t i = 0; i < di->elements_.size() - 1; ++i)
   {
      // Latitude and longitude coordinates in degrees
      const float lat1 = static_cast<float>(di->elements_[i].latitude_);
      const float lon1 = static_cast<float>(di->elements_[i].longitude_);
      const float lat2 = static_cast<float>(di->elements_[i + 1].latitude_);
      const float lon2 = static_cast<float>(di->elements_[i + 1].longitude_);

      // Calculate angle
      const units::angle::degrees<double> angle =
         util::GeographicLib::GetAngle(lat1, lon1, lat2, lon2);
      angles.push_back(angle);

      // Buffer line with hover text
      BufferLine(di,
                 di->elements_[i],
                 di->elements_[i + 1],
                 di->width_ + 2,
                 angle,
                 kBlack_,
                 thresholdValue,
                 startTime,
                 endTime,
                 true);
   }

   // For each element pair inside a Line statement, render a colored line
   for (std::size_t i = 0; i < di->elements_.size() - 1; ++i)
   {
      BufferLine(di,
                 di->elements_[i],
                 di->elements_[i + 1],
                 di->width_,
                 angles[i],
                 di->color_,
                 thresholdValue,
                 startTime,
                 endTime);
   }
}

void PlacefileLines::Impl::BufferLine(
   const std::shared_ptr<const gr::Placefile::LineDrawItem>& di,
   const gr::Placefile::LineDrawItem::Element&               e1,
   const gr::Placefile::LineDrawItem::Element&               e2,
   const float                                               width,
   const units::angle::degrees<double>                       angle,
   const boost::gil::rgba8_pixel_t                           color,
   const std::int32_t                                        threshold,
   const std::int32_t                                        startTime,
   const std::int32_t                                        endTime,
   bool                                                      bufferHover)
{
   // Latitude and longitude coordinates in degrees
   const float lat1 = static_cast<float>(e1.latitude_);
   const float lon1 = static_cast<float>(e1.longitude_);
   const float lat2 = static_cast<float>(e2.latitude_);
   const float lon2 = static_cast<float>(e2.longitude_);

   // TODO: Base X/Y offsets in pixels
   // const float x1 = static_cast<float>(e1.x_);
   // const float y1 = static_cast<float>(e1.y_);
   // const float x2 = static_cast<float>(e2.x_);
   // const float y2 = static_cast<float>(e2.y_);

   // Screen-space direction (map Y flipped). Bake into aXYOffset; aAngleDeg
   // slot holds unrotated perpendicular for stroke banding.
   const glm::vec2 sc1 =
      util::maplibre::LatLongToScreenCoordinate({lat1, lon1});
   const glm::vec2 sc2 =
      util::maplibre::LatLongToScreenCoordinate({lat2, lon2});
   glm::vec2 along {sc2.x - sc1.x, -(sc2.y - sc1.y)};
   const float alongLen = glm::length(along);
   if (alongLen > 1.0e-6f)
   {
      along /= alongLen;
   }
   else
   {
      along = glm::vec2 {0.0f, 1.0f};
   }
   const glm::vec2 perp {-along.y, along.x};

   // Final X/Y offsets in pixels
   const float hw = width * 0.5f;
   const float lx = -hw;
   const float rx = +hw;
   const glm::vec2 bl = perp * lx + along * (-hw);
   const glm::vec2 tl = perp * lx + along * (+hw);
   const glm::vec2 br = perp * rx + along * (-hw);
   const glm::vec2 tr = perp * rx + along * (+hw);
   const float     blX = bl.x;
   const float     blY = bl.y;
   const float     tlX = tl.x;
   const float     tlY = tl.y;
   const float     brX = br.x;
   const float     brY = br.y;
   const float     trX = tr.x;
   const float     trY = tr.y;

   // Modulate color
   const float mc0 = color[0] / 255.0f;
   const float mc1 = color[1] / 255.0f;
   const float mc2 = color[2] / 255.0f;
   const float mc3 = color[3] / 255.0f;

   // Update buffers
   const auto appendVertex =
      [&](float lat, float lon, float x, float y, float perp)
   {
      newLinesBuffer_.insert(newLinesBuffer_.end(),
                             {lat, lon, x, y, mc0, mc1, mc2, mc3, perp});
      newLinesBuffer_.insert(newLinesBuffer_.end(),
                             kNoStrokeVertexPadding.begin(),
                             kNoStrokeVertexPadding.end());
   };

   appendVertex(lat1, lon1, blX, blY, lx);
   appendVertex(lat2, lon2, tlX, tlY, lx);
   appendVertex(lat1, lon1, brX, brY, rx);
   appendVertex(lat1, lon1, brX, brY, rx);
   appendVertex(lat2, lon2, trX, trY, rx);
   appendVertex(lat2, lon2, tlX, tlY, lx);
   newIntegerBuffer_.insert(newIntegerBuffer_.end(),
                            {threshold,
                             startTime,
                             endTime,
                             threshold,
                             startTime,
                             endTime,
                             threshold,
                             startTime,
                             endTime,
                             threshold,
                             startTime,
                             endTime,
                             threshold,
                             startTime,
                             endTime,
                             threshold,
                             startTime,
                             endTime});

   if (bufferHover && !di->hoverText_.empty())
   {
      const units::angle::radians<double> radians = angle;

      const auto sc1 = util::maplibre::LatLongToScreenCoordinate({lat1, lon1});
      const auto sc2 = util::maplibre::LatLongToScreenCoordinate({lat2, lon2});

      const float cosAngle = cosf(static_cast<float>(radians.value()));
      const float sinAngle = sinf(static_cast<float>(radians.value()));

      const glm::mat2 rotate {cosAngle, -sinAngle, sinAngle, cosAngle};

      const glm::vec2 otl = rotate * glm::vec2 {-hw, +hw};
      const glm::vec2 otr = rotate * glm::vec2 {+hw, +hw};
      const glm::vec2 obl = rotate * glm::vec2 {-hw, -hw};
      const glm::vec2 obr = rotate * glm::vec2 {+hw, -hw};

      newHoverLines_.emplace_back(
         LineHoverEntry {di, sc1, sc2, otl, otr, obl, obr});
   }
}

void PlacefileLines::Impl::Update()
{
   dirty_ = false;
}

bool PlacefileLines::RunMousePicking(
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF& /* mouseLocalPos */,
   const QPointF&   mouseGlobalPos,
   const glm::vec2& mouseCoords,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
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

   // For each pickable line
   auto it = std::find_if(
      std::execution::par_unseq,
      p->currentHoverLines_.crbegin(),
      p->currentHoverLines_.crend(),
      [&mapDistance, &selectedTime, &mapMatrix, &mouseCoords](const auto& line)
      {
         if ((
                // Placefile is thresholded
                mapDistance > units::length::meters<double> {0.0} &&

                // Placefile threshold is < 999 nmi
                static_cast<int>(std::round(
                   units::length::nautical_miles<double> {line.di_->threshold_}
                      .value())) < 999 &&

                // Map distance is beyond/within the threshold
                line.di_->threshold_ < mapDistance &&
                (line.di_->threshold_.value() >= 0.0 ||
                 -(line.di_->threshold_) > mapDistance)) ||

             (
                // Line has a start time
                line.di_->startTime_ !=
                   std::chrono::system_clock::time_point {} &&

                // The time range has not yet started
                (selectedTime < line.di_->startTime_ ||

                 // The time range has ended
                 line.di_->endTime_ <= selectedTime)))
         {
            // Line is not pickable
            return false;
         }

         // Initialize vertices
         glm::vec2 bl = line.p1_;
         glm::vec2 br = bl;
         glm::vec2 tl = line.p2_;
         glm::vec2 tr = tl;

         // Calculate offsets
         // - Rotated offset is half the line width (pixels) in each direction
         // - Multiply the offset by the scaled and rotated map matrix
         const glm::vec2 otl = mapMatrix * glm::vec4 {line.otl_, 0.0f, 1.0f};
         const glm::vec2 obl = mapMatrix * glm::vec4 {line.obl_, 0.0f, 1.0f};
         const glm::vec2 obr = mapMatrix * glm::vec4 {line.obr_, 0.0f, 1.0f};
         const glm::vec2 otr = mapMatrix * glm::vec4 {line.otr_, 0.0f, 1.0f};

         // Offset vertices
         tl += otl;
         bl += obl;
         br += obr;
         tr += otr;

         // TODO: X/Y offsets

         // Test point against polygon bounds
         return util::maplibre::IsPointInPolygon({tl, bl, br, tr}, mouseCoords);
      });

   if (it != p->currentHoverLines_.crend())
   {
      itemPicked = true;
      util::tooltip::Show(it->di_->hoverText_, mouseGlobalPos);
   }

   return itemPicked;
}

} // namespace draw
} // namespace qt
} // namespace scwx
