#include <scwx/qt/gl/draw/placefile_icons.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#if defined(SCWX_RENDER_BACKEND_VULKAN)
#   include <scwx/qt/render/rhi_geo_uniforms.hpp>
#   include <scwx/qt/render/rhi_texture_array_overlay.hpp>
#   include <scwx/qt/render/rhi_vulkan_overlay.hpp>
#endif

#include <execution>

#include <QDir>
#include <QUrl>
#include <boost/unordered/unordered_flat_map.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_icons";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kNumRectangles        = 1;
static constexpr std::size_t kNumTriangles         = kNumRectangles * 2;
static constexpr std::size_t kVerticesPerTriangle  = 3;
static constexpr std::size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr std::size_t kPointsPerVertex      = 9;
static constexpr std::size_t kPointsPerTexCoord    = 3;
static constexpr std::size_t kIconBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerVertex;
static constexpr std::size_t kTextureBufferLength =
   kNumTriangles * kVerticesPerTriangle * kPointsPerTexCoord;

// Threshold, start time, end time
static constexpr std::size_t kIntegersPerVertex_ = 3;

#if defined(SCWX_RENDER_BACKEND_VULKAN)
static constexpr std::size_t kGeoFloatsPerVertex_ = 12;

static std::vector<float>
MergeGeoTextureVertices(const std::vector<float>& iconBuffer,
                        const std::vector<float>& textureBuffer)
{
   const std::size_t  vertexCount = iconBuffer.size() / kPointsPerVertex;
   std::vector<float> merged(vertexCount * kGeoFloatsPerVertex_);

   for (std::size_t v = 0; v < vertexCount; ++v)
   {
      const std::size_t iconOffset   = v * kPointsPerVertex;
      const std::size_t texOffset    = v * kPointsPerTexCoord;
      const std::size_t mergedOffset = v * kGeoFloatsPerVertex_;

      merged[mergedOffset + 0]  = iconBuffer[iconOffset + 0];
      merged[mergedOffset + 1]  = iconBuffer[iconOffset + 1];
      merged[mergedOffset + 2]  = iconBuffer[iconOffset + 2];
      merged[mergedOffset + 3]  = iconBuffer[iconOffset + 3];
      merged[mergedOffset + 4]  = textureBuffer[texOffset + 0];
      merged[mergedOffset + 5]  = textureBuffer[texOffset + 1];
      merged[mergedOffset + 6]  = textureBuffer[texOffset + 2];
      merged[mergedOffset + 7]  = iconBuffer[iconOffset + 4];
      merged[mergedOffset + 8]  = iconBuffer[iconOffset + 5];
      merged[mergedOffset + 9]  = iconBuffer[iconOffset + 6];
      merged[mergedOffset + 10] = iconBuffer[iconOffset + 7];
      merged[mergedOffset + 11] = iconBuffer[iconOffset + 8];
   }

   return merged;
}
#endif

struct PlacefileIconInfo
{
   PlacefileIconInfo(
      const std::shared_ptr<const gr::Placefile::IconFile>& iconFile,
      const std::string&                                    baseUrlString) :
       iconFile_ {iconFile}
   {
      // Resolve using base URL
      auto baseUrl = QUrl::fromUserInput(QString::fromStdString(baseUrlString));
      auto relativeUrl = QUrl(QDir::fromNativeSeparators(
         QString::fromStdString(iconFile->filename_)));
      resolvedUrl_     = baseUrl.resolved(relativeUrl).toString().toStdString();
   }

   void UpdateTextureInfo();

   std::string                                    resolvedUrl_;
   std::shared_ptr<const gr::Placefile::IconFile> iconFile_;
   util::TextureAttributes                        texture_ {};
   std::size_t                                    rows_ {};
   std::size_t                                    columns_ {};
   std::size_t                                    numIcons_ {};
   float                                          scaledWidth_ {};
   float                                          scaledHeight_ {};
};

class PlacefileIcons::Impl
{
public:
   struct IconHoverEntry
   {
      std::shared_ptr<const gr::Placefile::IconDrawItem> di_;

      glm::vec2 p_;
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

   void UpdateBuffers();
   void UpdateTextureBuffer();
   void Update(bool textureAtlasChanged);
#if defined(SCWX_RENDER_BACKEND_VULKAN)
   void UpdateVulkan(bool textureAtlasChanged);
#endif

   std::shared_ptr<render::RenderContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};
   bool lastTextureAtlasChanged_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex iconMutex_;

   boost::unordered_flat_map<std::size_t, PlacefileIconInfo>
      currentIconFiles_ {};
   boost::unordered_flat_map<std::size_t, PlacefileIconInfo> newIconFiles_ {};

   std::vector<std::shared_ptr<const gr::Placefile::IconDrawItem>>
      currentIconList_ {};
   std::vector<std::shared_ptr<const gr::Placefile::IconDrawItem>>
      newIconList_ {};
   std::vector<std::shared_ptr<const gr::Placefile::IconDrawItem>>
      newValidIconList_ {};

   std::vector<float>        currentIconBuffer_ {};
   std::vector<std::int32_t> currentIntegerBuffer_ {};
   std::vector<float>        newIconBuffer_ {};
   std::vector<std::int32_t> newIntegerBuffer_ {};

   std::vector<float> textureBuffer_ {};

   std::vector<IconHoverEntry> currentHoverIcons_ {};
   std::vector<IconHoverEntry> newHoverIcons_ {};

   std::uint32_t numVertices_;
};

PlacefileIcons::PlacefileIcons(
   const std::shared_ptr<render::RenderContext>& context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
PlacefileIcons::~PlacefileIcons() = default;

PlacefileIcons::PlacefileIcons(PlacefileIcons&&) noexcept            = default;
PlacefileIcons& PlacefileIcons::operator=(PlacefileIcons&&) noexcept = default;

void PlacefileIcons::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void PlacefileIcons::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileIcons::Initialize() {}

void PlacefileIcons::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   bool /* textureAtlasChanged */)
{
}

#if defined(SCWX_RENDER_BACKEND_VULKAN)
void PlacefileIcons::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool                                          textureAtlasChanged)
{
   std::unique_lock lock {p->iconMutex_};

   if (p->currentIconList_.empty())
   {
      return;
   }

   p->UpdateVulkan(textureAtlasChanged);

   resources.textureArrayOverlay.SyncAtlas(commandBuffer,
                                           p->context_->texture_buffer_count());

   const std::vector<float> mergedVertices =
      MergeGeoTextureVertices(p->currentIconBuffer_, p->textureBuffer_);

   std::vector<std::int32_t> integerVertices;
   integerVertices.reserve(p->currentIntegerBuffer_.size() /
                           kIntegersPerVertex_ * 4);
   for (std::size_t i = 0; i < p->currentIntegerBuffer_.size();
        i += kIntegersPerVertex_)
   {
      integerVertices.push_back(p->currentIntegerBuffer_[i]);
      integerVertices.push_back(p->currentIntegerBuffer_[i + 1]);
      integerVertices.push_back(p->currentIntegerBuffer_[i + 2]);
      integerVertices.push_back(1);
   }

   const scwx::qt::render::GeoUniforms uniforms =
      scwx::qt::render::BuildGeoUniforms(
         params, p->thresholded_, p->selectedTime_);

   resources.textureArrayOverlay.RenderGeo(
      commandBuffer,
      uniforms,
      mergedVertices,
      integerVertices,
      static_cast<std::uint32_t>(p->numVertices_));
}
#endif

void PlacefileIcons::Deinitialize()
{
   std::unique_lock lock {p->iconMutex_};

   p->currentIconList_.clear();
   p->currentIconFiles_.clear();
   p->currentHoverIcons_.clear();
   p->currentIconBuffer_.clear();
   p->currentIntegerBuffer_.clear();
   p->textureBuffer_.clear();
}

void PlacefileIconInfo::UpdateTextureInfo()
{
   texture_ = util::TextureAtlas::Instance().GetTextureAttributes(resolvedUrl_);

   if (iconFile_->iconWidth_ > 0 && iconFile_->iconHeight_ > 0)
   {
      columns_ = texture_.size_.x / iconFile_->iconWidth_;
      rows_    = texture_.size_.y / iconFile_->iconHeight_;
   }
   else
   {
      columns_ = 0u;
      rows_    = 0u;
   }

   numIcons_ = columns_ * rows_;

   // Pixel size
   float xFactor = 0.0f;
   float yFactor = 0.0f;

   if (texture_.size_.x > 0 && texture_.size_.y > 0)
   {
      xFactor = (texture_.sRight_ - texture_.sLeft_) / texture_.size_.x;
      yFactor = (texture_.tBottom_ - texture_.tTop_) / texture_.size_.y;
   }

   scaledWidth_  = iconFile_->iconWidth_ * xFactor;
   scaledHeight_ = iconFile_->iconHeight_ * yFactor;
}

void PlacefileIcons::StartIcons()
{
   // Clear the new buffer
   p->newIconList_.clear();
   p->newValidIconList_.clear();
   p->newIconFiles_.clear();
   p->newIconBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverIcons_.clear();
}

void PlacefileIcons::SetIconFiles(
   const std::vector<std::shared_ptr<const gr::Placefile::IconFile>>& iconFiles,
   const std::string&                                                 baseUrl)
{
   // Populate icon file map
   for (auto& file : iconFiles)
   {
      p->newIconFiles_.emplace(
         std::piecewise_construct,
         std::tuple {file->fileNumber_},
         std::forward_as_tuple(PlacefileIconInfo {file, baseUrl}));
   }
}

void PlacefileIcons::AddIcon(
   const std::shared_ptr<gr::Placefile::IconDrawItem>& di)
{
   if (di != nullptr)
   {
      p->newIconList_.emplace_back(di);
   }
}

void PlacefileIcons::FinishIcons()
{
   // Update icon files
   for (auto& iconFile : p->newIconFiles_)
   {
      iconFile.second.UpdateTextureInfo();
   }

   // Update buffers
   p->UpdateBuffers();

   std::unique_lock lock {p->iconMutex_};

   // Swap buffers
   p->currentIconList_.swap(p->newValidIconList_);
   p->currentIconFiles_.swap(p->newIconFiles_);
   p->currentIconBuffer_.swap(p->newIconBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);
   p->currentHoverIcons_.swap(p->newHoverIcons_);

   // Clear the new buffers
   p->newIconList_.clear();
   p->newValidIconList_.clear();
   p->newIconFiles_.clear();
   p->newIconBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverIcons_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileIcons::Impl::UpdateBuffers()
{
   newIconBuffer_.clear();
   newIconBuffer_.reserve(newIconList_.size() * kIconBufferLength);
   newIntegerBuffer_.clear();
   newIntegerBuffer_.reserve(newIconList_.size() * kVerticesPerRectangle *
                             kIntegersPerVertex_);

   for (auto& di : newIconList_)
   {
      auto it = newIconFiles_.find(di->fileNumber_);
      if (it == newIconFiles_.cend())
      {
         // No file found
         logger_->trace("Could not find file number: {}", di->fileNumber_);
         continue;
      }

      auto& icon = it->second;

      // Validate icon
      if (di->iconNumber_ == 0 || di->iconNumber_ > icon.numIcons_)
      {
         // No icon found
         logger_->trace("Invalid icon number: {}", di->iconNumber_);
         continue;
      }

      // Icon is valid, add to valid icon list
      newValidIconList_.push_back(di);

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
      const float lat = static_cast<float>(di->latitude_);
      const float lon = static_cast<float>(di->longitude_);

      // Base X/Y offsets in pixels
      const float x = static_cast<float>(di->x_);
      const float y = static_cast<float>(di->y_);

      // Icon size
      const float iw = static_cast<float>(icon.iconFile_->iconWidth_);
      const float ih = static_cast<float>(icon.iconFile_->iconHeight_);

      // Hot X/Y (zero-based icon center)
      const float hx = static_cast<float>(icon.iconFile_->hotX_);
      const float hy = static_cast<float>(icon.iconFile_->hotY_);

      // Final X/Y offsets in pixels
      const float lx = std::roundf(x - hx);
      const float rx = std::roundf(lx + iw);
      const float ty = std::roundf(y + hy);
      const float by = std::roundf(ty - ih);

      // Angle in degrees
      units::angle::degrees<float> angle = di->angle_;
      const float                  a     = angle.value();

      // Modulate color
      const float mc0 = di->modulate_[0] / 255.0f;
      const float mc1 = di->modulate_[1] / 255.0f;
      const float mc2 = di->modulate_[2] / 255.0f;
      const float mc3 = di->modulate_[3] / 255.0f;

      newIconBuffer_.insert(newIconBuffer_.end(),
                            {
                               // Icon
                               lat, lon, lx, by, mc0, mc1, mc2, mc3, a, // BL
                               lat, lon, lx, ty, mc0, mc1, mc2, mc3, a, // TL
                               lat, lon, rx, by, mc0, mc1, mc2, mc3, a, // BR
                               lat, lon, rx, by, mc0, mc1, mc2, mc3, a, // BR
                               lat, lon, rx, ty, mc0, mc1, mc2, mc3, a, // TR
                               lat, lon, lx, ty, mc0, mc1, mc2, mc3, a  // TL
                            });
      newIntegerBuffer_.insert(newIntegerBuffer_.end(),
                               {thresholdValue,
                                startTime,
                                endTime,
                                thresholdValue,
                                startTime,
                                endTime,
                                thresholdValue,
                                startTime,
                                endTime,
                                thresholdValue,
                                startTime,
                                endTime,
                                thresholdValue,
                                startTime,
                                endTime,
                                thresholdValue,
                                startTime,
                                endTime});

      if (!di->hoverText_.empty())
      {
         const units::angle::radians<double> radians = angle;

         const auto sc = util::maplibre::LatLongToScreenCoordinate({lat, lon});

         const float cosAngle = cosf(static_cast<float>(radians.value()));
         const float sinAngle = sinf(static_cast<float>(radians.value()));

         const glm::mat2 rotate {cosAngle, -sinAngle, sinAngle, cosAngle};

         const glm::vec2 otl = rotate * glm::vec2 {lx, ty};
         const glm::vec2 otr = rotate * glm::vec2 {rx, ty};
         const glm::vec2 obl = rotate * glm::vec2 {lx, by};
         const glm::vec2 obr = rotate * glm::vec2 {rx, by};

         newHoverIcons_.emplace_back(
            IconHoverEntry {di, sc, otl, otr, obl, obr});
      }
   }
}

void PlacefileIcons::Impl::UpdateTextureBuffer()
{
   textureBuffer_.clear();
   textureBuffer_.reserve(currentIconList_.size() * kTextureBufferLength);

   for (auto& di : currentIconList_)
   {
      auto it = currentIconFiles_.find(di->fileNumber_);
      if (it == currentIconFiles_.cend())
      {
         // No file found. Should not get here, but insert empty data to match
         // up with data already buffered
         logger_->error("Could not find file number: {}", di->fileNumber_);

         // clang-format off
         textureBuffer_.insert(
            textureBuffer_.end(),
            {
               // Icon
               0.0f, 0.0f, 0.0f, // BL
               0.0f, 0.0f, 0.0f, // TL
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // TR
               0.0f, 0.0f, 0.0f  // TL
            });
         // clang-format on

         continue;
      }

      auto& icon = it->second;

      // Validate icon
      if (di->iconNumber_ == 0 || di->iconNumber_ > icon.numIcons_)
      {
         // No icon found
         logger_->trace("Invalid icon number: {}", di->iconNumber_);

         // Will get here if a texture changes, and the texture shrunk such that
         // the icon is no longer found

         // clang-format off
         textureBuffer_.insert(
            textureBuffer_.end(),
            {
               // Icon
               0.0f, 0.0f, 0.0f, // BL
               0.0f, 0.0f, 0.0f, // TL
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // BR
               0.0f, 0.0f, 0.0f, // TR
               0.0f, 0.0f, 0.0f  // TL
            });
         // clang-format on

         continue;
      }

      // Texture coordinates
      const std::size_t iconRow    = (di->iconNumber_ - 1) / icon.columns_;
      const std::size_t iconColumn = (di->iconNumber_ - 1) % icon.columns_;

      const float iconX = iconColumn * icon.scaledWidth_;
      const float iconY = iconRow * icon.scaledHeight_;

      const float ls = icon.texture_.sLeft_ + iconX;
      const float rs = ls + icon.scaledWidth_;
      const float tt = icon.texture_.tTop_ + iconY;
      const float bt = tt + icon.scaledHeight_;
      const float r  = static_cast<float>(icon.texture_.layerId_);

      // clang-format off
      textureBuffer_.insert(
         textureBuffer_.end(),
         {
            // Icon
            ls, bt, r, // BL
            ls, tt, r, // TL
            rs, bt, r, // BR
            rs, bt, r, // BR
            rs, tt, r, // TR
            ls, tt, r  // TL
         });
      // clang-format on
   }
}

void PlacefileIcons::Impl::Update(bool textureAtlasChanged)
{
   // If the texture atlas has changed
   if (dirty_ || textureAtlasChanged)
   {
      // Update texture coordinates
      for (auto& iconFile : currentIconFiles_)
      {
         iconFile.second.UpdateTextureInfo();
      }

      // Update icon texture buffer data
      UpdateTextureBuffer();
   }

   // If buffers need updating
   if (dirty_)
   {
      numVertices_ = static_cast<std::uint32_t>(currentIconBuffer_.size() /
                                                kPointsPerVertex);
   }

   dirty_ = false;
}

#if defined(SCWX_RENDER_BACKEND_VULKAN)
void PlacefileIcons::Impl::UpdateVulkan(bool textureAtlasChanged)
{
   if (dirty_ || textureAtlasChanged)
   {
      for (auto& iconFile : currentIconFiles_)
      {
         iconFile.second.UpdateTextureInfo();
      }

      UpdateTextureBuffer();
   }

   if (dirty_)
   {
      numVertices_ = static_cast<std::uint32_t>(currentIconBuffer_.size() /
                                                kPointsPerVertex);
   }

   dirty_ = false;
}
#endif

bool PlacefileIcons::RunMousePicking(
   const QMapLibre::CustomLayerRenderParameters& params,
   const QPointF& /* mouseLocalPos */,
   const QPointF&   mouseGlobalPos,
   const glm::vec2& mouseCoords,
   const common::Coordinate& /* mouseGeoCoords */,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   std::unique_lock lock {p->iconMutex_};

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

   // For each pickable icon
   auto it = std::find_if(
      std::execution::par_unseq,
      p->currentHoverIcons_.crbegin(),
      p->currentHoverIcons_.crend(),
      [&mapDistance, &selectedTime, &mapMatrix, &mouseCoords](const auto& icon)
      {
         if ((
                // Placefile is thresholded
                mapDistance > units::length::meters<double> {0.0} &&

                // Placefile threshold is < 999 nmi
                static_cast<int>(std::round(
                   units::length::nautical_miles<double> {icon.di_->threshold_}
                      .value())) < 999 &&

                // Map distance is beyond/within the threshold
                icon.di_->threshold_ < mapDistance &&
                (icon.di_->threshold_.value() >= 0.0 ||
                 -(icon.di_->threshold_) > mapDistance)) ||

             (
                // Line has a start time
                icon.di_->startTime_ !=
                   std::chrono::system_clock::time_point {} &&

                // The time range has not yet started
                (selectedTime < icon.di_->startTime_ ||

                 // The time range has ended
                 icon.di_->endTime_ <= selectedTime)))
         {
            // Icon is not pickable
            return false;
         }

         // Initialize vertices
         glm::vec2 bl = icon.p_;
         glm::vec2 br = bl;
         glm::vec2 tl = br;
         glm::vec2 tr = tl;

         // Calculate offsets
         // - Rotated offset is based on final X/Y offsets (pixels)
         // - Multiply the offset by the scaled and rotated map matrix
         const glm::vec2 otl = mapMatrix * glm::vec4 {icon.otl_, 0.0f, 1.0f};
         const glm::vec2 obl = mapMatrix * glm::vec4 {icon.obl_, 0.0f, 1.0f};
         const glm::vec2 obr = mapMatrix * glm::vec4 {icon.obr_, 0.0f, 1.0f};
         const glm::vec2 otr = mapMatrix * glm::vec4 {icon.otr_, 0.0f, 1.0f};

         // Offset vertices
         tl += otl;
         bl += obl;
         br += obr;
         tr += otr;

         // Test point against polygon bounds
         return util::maplibre::IsPointInPolygon({tl, bl, br, tr}, mouseCoords);
      });

   if (it != p->currentHoverIcons_.crend())
   {
      itemPicked = true;
      util::tooltip::Show(it->di_->hoverText_, mouseGlobalPos);
   }

   return itemPicked;
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
