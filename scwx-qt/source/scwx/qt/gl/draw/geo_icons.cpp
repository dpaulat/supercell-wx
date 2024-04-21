#include <scwx/qt/gl/draw/geo_icons.hpp>
#include <scwx/qt/types/icon_types.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>

#include <execution>

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::geo_icons";
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

// Threshold, start time, end time, displayed
static constexpr std::size_t kIntegersPerVertex_ = 4;
static constexpr std::size_t kIntegerBufferLength_ =
   kNumTriangles * kVerticesPerTriangle * kIntegersPerVertex_;

struct GeoIconDrawItem
{
   units::length::nautical_miles<double>       threshold_ {};
   std::chrono::sys_time<std::chrono::seconds> startTime_ {};
   std::chrono::sys_time<std::chrono::seconds> endTime_ {};

   boost::gil::rgba32f_pixel_t modulate_ {1.0f, 1.0f, 1.0f, 1.0f};
   bool                        visible_ {true};
   double                      latitude_ {};
   double                      longitude_ {};
   double                      x_ {};
   double                      y_ {};
   units::degrees<double>      angle_ {};
   std::string                 iconSheet_ {};
   std::size_t                 iconIndex_ {};
   std::string                 hoverText_ {};

   std::shared_ptr<types::IconInfo> iconInfo_ {};
};

class GeoIcons::Impl
{
public:
   struct IconHoverEntry
   {
      std::shared_ptr<GeoIconDrawItem> di_;

      glm::vec2 p_;
      glm::vec2 otl_;
      glm::vec2 otr_;
      glm::vec2 obl_;
      glm::vec2 obr_;
   };

   explicit Impl(const std::shared_ptr<GlContext>& context) :
       context_ {context},
       shaderProgram_ {nullptr},
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       uMapDistanceLocation_(GL_INVALID_INDEX),
       uSelectedTimeLocation_(GL_INVALID_INDEX),
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX},
       numVertices_ {0}
   {
   }

   ~Impl() {}

   void        UpdateBuffers();
   static void UpdateSingleBuffer(const std::shared_ptr<GeoIconDrawItem>& di,
                                  std::size_t                  iconIndex,
                                  std::vector<float>&          iconBuffer,
                                  std::vector<GLint>&          integerBuffer,
                                  std::vector<IconHoverEntry>& hoverIcons);
   void        UpdateTextureBuffer();
   void        UpdateModifiedIconBuffers();
   void        Update(bool textureAtlasChanged);

   std::shared_ptr<GlContext> context_;

   bool visible_ {true};
   bool dirty_ {false};
   bool thresholded_ {false};
   bool lastTextureAtlasChanged_ {false};

   boost::unordered_flat_set<std::shared_ptr<GeoIconDrawItem>> dirtyIcons_ {};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex iconMutex_;

   boost::unordered_flat_map<std::string, std::shared_ptr<types::IconInfo>>
      currentIconSheets_ {};
   boost::unordered_flat_map<std::string, std::shared_ptr<types::IconInfo>>
      newIconSheets_ {};

   std::vector<std::shared_ptr<GeoIconDrawItem>> currentIconList_ {};
   std::vector<std::shared_ptr<GeoIconDrawItem>> newIconList_ {};
   std::vector<std::shared_ptr<GeoIconDrawItem>> newValidIconList_ {};

   std::vector<float> currentIconBuffer_ {};
   std::vector<GLint> currentIntegerBuffer_ {};
   std::vector<float> newIconBuffer_ {};
   std::vector<GLint> newIntegerBuffer_ {};

   std::vector<float> textureBuffer_ {};

   std::vector<IconHoverEntry> currentHoverIcons_ {};
   std::vector<IconHoverEntry> newHoverIcons_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;
   GLint                          uSelectedTimeLocation_;

   GLuint                vao_;
   std::array<GLuint, 3> vbo_;

   GLsizei numVertices_;
};

GeoIcons::GeoIcons(const std::shared_ptr<GlContext>& context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
{
}
GeoIcons::~GeoIcons() = default;

GeoIcons::GeoIcons(GeoIcons&&) noexcept            = default;
GeoIcons& GeoIcons::operator=(GeoIcons&&) noexcept = default;

void GeoIcons::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void GeoIcons::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void GeoIcons::Initialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/geo_texture2d.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/texture2d_array.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");
   p->uMapMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMapMatrix");
   p->uMapScreenCoordLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapScreenCoord");
   p->uMapDistanceLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapDistance");
   p->uSelectedTimeLocation_ =
      p->shaderProgram_->GetUniformLocation("uSelectedTime");

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aLatLong
   gl.glVertexAttribPointer(0,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // aXYOffset
   gl.glVertexAttribPointer(1,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(2 * sizeof(float)));
   gl.glEnableVertexAttribArray(1);

   // aModulate
   gl.glVertexAttribPointer(3,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(4 * sizeof(float)));
   gl.glEnableVertexAttribArray(3);

   // aAngle
   gl.glVertexAttribPointer(4,
                            1,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(8 * sizeof(float)));
   gl.glEnableVertexAttribArray(4);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aTexCoord
   gl.glVertexAttribPointer(2,
                            3,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerTexCoord * sizeof(float),
                            static_cast<void*>(0));
   gl.glEnableVertexAttribArray(2);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[2]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aThreshold
   gl.glVertexAttribIPointer(5, //
                             1,
                             GL_INT,
                             0,
                             static_cast<void*>(0));
   gl.glEnableVertexAttribArray(5);

   // aTimeRange
   gl.glVertexAttribIPointer(6, //
                             2,
                             GL_INT,
                             kIntegersPerVertex_ * sizeof(GLint),
                             reinterpret_cast<void*>(1 * sizeof(GLint)));
   gl.glEnableVertexAttribArray(6);

   // aDisplayed
   gl.glVertexAttribPointer(7,
                            1,
                            GL_INT,
                            GL_FALSE,
                            kIntegersPerVertex_ * sizeof(GLint),
                            reinterpret_cast<void*>(3 * sizeof(float)));
   gl.glEnableVertexAttribArray(7);

   p->dirty_ = true;
}

void GeoIcons::Render(const QMapLibre::CustomLayerRenderParameters& params,
                      bool textureAtlasChanged)
{
   if (!p->visible_)
   {
      if (textureAtlasChanged)
      {
         p->lastTextureAtlasChanged_ = true;
      }

      return;
   }

   std::unique_lock lock {p->iconMutex_};

   if (!p->currentIconList_.empty())
   {
      gl::OpenGLFunctions& gl = p->context_->gl();

      gl.glBindVertexArray(p->vao_);

      p->Update(textureAtlasChanged);
      p->shaderProgram_->Use();
      UseRotationProjection(params, p->uMVPMatrixLocation_);
      UseMapProjection(
         params, p->uMapMatrixLocation_, p->uMapScreenCoordLocation_);

      if (p->thresholded_)
      {
         // If thresholding is enabled, set the map distance
         units::length::nautical_miles<float> mapDistance =
            util::maplibre::GetMapDistance(params);
         gl.glUniform1f(p->uMapDistanceLocation_, mapDistance.value());
      }
      else
      {
         // If thresholding is disabled, set the map distance to 0
         gl.glUniform1f(p->uMapDistanceLocation_, 0.0f);
      }

      // Selected time
      std::chrono::system_clock::time_point selectedTime =
         (p->selectedTime_ == std::chrono::system_clock::time_point {}) ?
            std::chrono::system_clock::now() :
            p->selectedTime_;
      gl.glUniform1i(
         p->uSelectedTimeLocation_,
         static_cast<GLint>(std::chrono::duration_cast<std::chrono::minutes>(
                               selectedTime.time_since_epoch())
                               .count()));

      // Interpolate texture coordinates
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl.glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // Draw icons
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void GeoIcons::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   std::unique_lock lock {p->iconMutex_};

   p->currentIconList_.clear();
   p->currentIconSheets_.clear();
   p->currentHoverIcons_.clear();
   p->currentIconBuffer_.clear();
   p->currentIntegerBuffer_.clear();
   p->textureBuffer_.clear();
}

void GeoIcons::SetVisible(bool visible)
{
   p->visible_ = visible;
}

void GeoIcons::StartIconSheets()
{
   // Clear the new buffer
   p->newIconSheets_.clear();
}

void GeoIcons::AddIconSheet(const std::string& name,
                            std::size_t        iconWidth,
                            std::size_t        iconHeight,
                            std::int32_t       hotX,
                            std::int32_t       hotY)
{
   // Populate icon sheet map
   p->newIconSheets_.emplace(
      std::piecewise_construct,
      std::tuple {name},
      std::forward_as_tuple(std::make_shared<types::IconInfo>(
         name, iconWidth, iconHeight, hotX, hotY)));
}

void GeoIcons::FinishIconSheets()
{
   // Update icon sheets
   for (auto& iconSheet : p->newIconSheets_)
   {
      iconSheet.second->UpdateTextureInfo();
   }

   std::unique_lock lock {p->iconMutex_};

   // Swap buffers
   p->currentIconSheets_.swap(p->newIconSheets_);

   // Clear the new buffers
   p->newIconSheets_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void GeoIcons::StartIcons()
{
   // Clear the new buffer
   p->newIconList_.clear();
   p->newValidIconList_.clear();
   p->newIconBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverIcons_.clear();
}

std::shared_ptr<GeoIconDrawItem> GeoIcons::AddIcon()
{
   return p->newIconList_.emplace_back(std::make_shared<GeoIconDrawItem>());
}

void GeoIcons::SetIconVisible(const std::shared_ptr<GeoIconDrawItem>& di,
                              bool                                    visible)
{
   if (di->visible_ != visible)
   {
      di->visible_ = visible;
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::SetIconTexture(const std::shared_ptr<GeoIconDrawItem>& di,
                              const std::string&                      iconSheet,
                              std::size_t                             iconIndex)
{
   if (di->iconSheet_ != iconSheet || di->iconIndex_ != iconIndex)
   {
      di->iconSheet_ = iconSheet;
      di->iconIndex_ = iconIndex;
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::SetIconLocation(const std::shared_ptr<GeoIconDrawItem>& di,
                               units::angle::degrees<double>           latitude,
                               units::angle::degrees<double> longitude,
                               double                        xOffset,
                               double                        yOffset)
{
   if (di->latitude_ != latitude.value() ||
       di->longitude_ != longitude.value() || di->x_ != xOffset ||
       di->y_ != yOffset)
   {
      di->latitude_  = latitude.value();
      di->longitude_ = longitude.value();
      di->x_         = xOffset;
      di->y_         = yOffset;
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::SetIconLocation(const std::shared_ptr<GeoIconDrawItem>& di,
                               double                                  latitude,
                               double longitude,
                               double xOffset,
                               double yOffset)
{
   if (di->latitude_ != latitude || di->longitude_ != longitude ||
       di->x_ != xOffset || di->y_ != yOffset)
   {
      di->latitude_  = latitude;
      di->longitude_ = longitude;
      di->x_         = xOffset;
      di->y_         = yOffset;
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::SetIconAngle(const std::shared_ptr<GeoIconDrawItem>& di,
                            units::angle::degrees<double>           angle)
{
   if (di->angle_ != angle)
   {
      di->angle_ = angle;
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::SetIconModulate(const std::shared_ptr<GeoIconDrawItem>& di,
                               boost::gil::rgba8_pixel_t               modulate)
{
   boost::gil::rgba32f_pixel_t newModulate = {modulate[0] / 255.0f,
                                              modulate[1] / 255.0f,
                                              modulate[2] / 255.0f,
                                              modulate[3] / 255.0f};

   if (di->modulate_ != newModulate)
   {
      di->modulate_ = {modulate[0] / 255.0f,
                       modulate[1] / 255.0f,
                       modulate[2] / 255.0f,
                       modulate[3] / 255.0f};
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::SetIconModulate(const std::shared_ptr<GeoIconDrawItem>& di,
                               boost::gil::rgba32f_pixel_t             modulate)
{
   if (di->modulate_ != modulate)
   {
      di->modulate_ = modulate;
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::SetIconHoverText(const std::shared_ptr<GeoIconDrawItem>& di,
                                const std::string&                      text)
{
   if (di->hoverText_ != text)
   {
      di->hoverText_ = text;
      p->dirtyIcons_.insert(di);
   }
}

void GeoIcons::FinishIcons()
{
   // Update buffers
   p->UpdateBuffers();

   std::unique_lock lock {p->iconMutex_};

   // Swap buffers
   p->currentIconList_.swap(p->newValidIconList_);
   p->currentIconBuffer_.swap(p->newIconBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);
   p->currentHoverIcons_.swap(p->newHoverIcons_);

   // Clear the new buffers, except the full icon list (used to update buffers
   // without re-adding icons)
   p->newValidIconList_.clear();
   p->newIconBuffer_.clear();
   p->newIntegerBuffer_.clear();
   p->newHoverIcons_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void GeoIcons::Impl::UpdateBuffers()
{
   newIconBuffer_.clear();
   newIconBuffer_.reserve(newIconList_.size() * kIconBufferLength);
   newIntegerBuffer_.clear();
   newIntegerBuffer_.reserve(newIconList_.size() * kVerticesPerRectangle *
                             kIntegersPerVertex_);
   newValidIconList_.clear();
   newHoverIcons_.clear();

   for (auto& di : newIconList_)
   {
      auto it = currentIconSheets_.find(di->iconSheet_);
      if (it == currentIconSheets_.cend())
      {
         // No icon sheet found
         logger_->warn("Could not find icon sheet: {}", di->iconSheet_);
         continue;
      }

      auto& icon    = it->second;
      di->iconInfo_ = icon;

      // Validate icon
      if (di->iconIndex_ >= icon->numIcons_)
      {
         // No icon found
         logger_->warn("Invalid icon index: {}", di->iconIndex_);
         continue;
      }

      // Icon is valid, add to valid icon list
      newValidIconList_.push_back(di);

      // Update icon buffer
      UpdateSingleBuffer(di,
                         newValidIconList_.size() - 1,
                         newIconBuffer_,
                         newIntegerBuffer_,
                         newHoverIcons_);
   }

   // All icons have been updated
   dirtyIcons_.clear();
}

void GeoIcons::Impl::UpdateSingleBuffer(
   const std::shared_ptr<GeoIconDrawItem>& di,
   std::size_t                             iconIndex,
   std::vector<float>&                     iconBuffer,
   std::vector<GLint>&                     integerBuffer,
   std::vector<IconHoverEntry>&            hoverIcons)
{
   auto& icon = di->iconInfo_;

   // Threshold value
   units::length::nautical_miles<double> threshold = di->threshold_;
   GLint thresholdValue = static_cast<GLint>(std::round(threshold.value()));

   // Start and end time
   GLint startTime =
      static_cast<GLint>(std::chrono::duration_cast<std::chrono::minutes>(
                            di->startTime_.time_since_epoch())
                            .count());
   GLint endTime =
      static_cast<GLint>(std::chrono::duration_cast<std::chrono::minutes>(
                            di->endTime_.time_since_epoch())
                            .count());

   // Latitude and longitude coordinates in degrees
   const float lat = static_cast<float>(di->latitude_);
   const float lon = static_cast<float>(di->longitude_);

   // Base X/Y offsets in pixels
   const float x = static_cast<float>(di->x_);
   const float y = static_cast<float>(di->y_);

   // Icon size
   const float iw = static_cast<float>(icon->iconWidth_);
   const float ih = static_cast<float>(icon->iconHeight_);

   // Hot X/Y (zero-based icon center)
   const float hx = static_cast<float>(icon->hotX_);
   const float hy = static_cast<float>(icon->hotY_);

   // Final X/Y offsets in pixels
   const float lx = std::roundf(x - hx);
   const float rx = std::roundf(lx + iw);
   const float ty = std::roundf(y + hy);
   const float by = std::roundf(ty - ih);

   // Angle in degrees
   units::angle::degrees<float> angle = di->angle_;
   const float                  a     = angle.value();

   // Modulate color
   const float mc0 = di->modulate_[0];
   const float mc1 = di->modulate_[1];
   const float mc2 = di->modulate_[2];
   const float mc3 = di->modulate_[3];

   // Visibility
   const GLint v = static_cast<GLint>(di->visible_);

   // Icon initialize list data
   const auto iconData = {
      // Icon
      lat, lon, lx, by, mc0, mc1, mc2, mc3, a, // BL
      lat, lon, lx, ty, mc0, mc1, mc2, mc3, a, // TL
      lat, lon, rx, by, mc0, mc1, mc2, mc3, a, // BR
      lat, lon, rx, by, mc0, mc1, mc2, mc3, a, // BR
      lat, lon, rx, ty, mc0, mc1, mc2, mc3, a, // TR
      lat, lon, lx, ty, mc0, mc1, mc2, mc3, a  // TL
   };
   const auto integerData = {thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v,
                             thresholdValue, startTime, endTime, v};

   // Buffer position data
   auto iconBufferPosition = iconBuffer.end();
   auto iconBufferOffset   = iconIndex * kIconBufferLength;

   auto integerBufferPosition = integerBuffer.end();
   auto integerBufferOffset   = iconIndex * kIntegerBufferLength_;

   if (iconBufferOffset < iconBuffer.size())
   {
      iconBufferPosition = iconBuffer.begin() + iconBufferOffset;
   }
   if (integerBufferOffset < integerBuffer.size())
   {
      integerBufferPosition = integerBuffer.begin() + integerBufferOffset;
   }

   if (iconBufferPosition == iconBuffer.cend())
   {
      iconBuffer.insert(iconBufferPosition, iconData);
   }
   else
   {
      std::copy(iconData.begin(), iconData.end(), iconBufferPosition);
   }

   if (integerBufferPosition == integerBuffer.cend())
   {
      integerBuffer.insert(integerBufferPosition, integerData);
   }
   else
   {
      std::copy(integerData.begin(), integerData.end(), integerBufferPosition);
   }

   auto hoverIt = std::find_if(hoverIcons.begin(),
                               hoverIcons.end(),
                               [&di](auto& entry) { return entry.di_ == di; });

   if (di->visible_ && !di->hoverText_.empty())
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

      if (hoverIt == hoverIcons.end())
      {
         hoverIcons.emplace_back(IconHoverEntry {di, sc, otl, otr, obl, obr});
      }
      else
      {
         hoverIt->otl_ = otl;
         hoverIt->otr_ = otr;
         hoverIt->obl_ = obl;
         hoverIt->obr_ = obr;
      }
   }
   else if (hoverIt != hoverIcons.end())
   {
      hoverIcons.erase(hoverIt);
   }
}

void GeoIcons::Impl::UpdateTextureBuffer()
{
   textureBuffer_.clear();
   textureBuffer_.reserve(currentIconList_.size() * kTextureBufferLength);

   for (auto& di : currentIconList_)
   {
      auto it = currentIconSheets_.find(di->iconSheet_);
      if (it == currentIconSheets_.cend())
      {
         // No file found. Should not get here, but insert empty data to match
         // up with data already buffered
         logger_->error("Could not find icon sheet: {}", di->iconSheet_);

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
      if (di->iconIndex_ >= icon->numIcons_)
      {
         // No icon found
         logger_->error("Invalid icon index: {}", di->iconIndex_);

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
      const std::size_t iconRow    = (di->iconIndex_) / icon->columns_;
      const std::size_t iconColumn = (di->iconIndex_) % icon->columns_;

      const float iconX = iconColumn * icon->scaledWidth_;
      const float iconY = iconRow * icon->scaledHeight_;

      const float ls = icon->texture_.sLeft_ + iconX;
      const float rs = ls + icon->scaledWidth_;
      const float tt = icon->texture_.tTop_ + iconY;
      const float bt = tt + icon->scaledHeight_;
      const float r  = static_cast<float>(icon->texture_.layerId_);

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

void GeoIcons::Impl::UpdateModifiedIconBuffers()
{
   // Update buffers for modified icons
   for (auto& di : dirtyIcons_)
   {
      // Find modified icon in the current list
      auto it =
         std::find(currentIconList_.cbegin(), currentIconList_.cend(), di);

      // Ignore invalid icons
      if (it == currentIconList_.cend())
      {
         continue;
      }

      auto iconIndex = std::distance(currentIconList_.cbegin(), it);

      UpdateSingleBuffer(di,
                         iconIndex,
                         currentIconBuffer_,
                         currentIntegerBuffer_,
                         currentHoverIcons_);
   }

   // Clear list of modified icons
   if (!dirtyIcons_.empty())
   {
      dirtyIcons_.clear();
      dirty_ = true;
   }
}

void GeoIcons::Impl::Update(bool textureAtlasChanged)
{
   gl::OpenGLFunctions& gl = context_->gl();

   UpdateModifiedIconBuffers();

   // If the texture atlas has changed
   if (dirty_ || textureAtlasChanged || lastTextureAtlasChanged_)
   {
      // Update texture coordinates
      for (auto& iconSheet : currentIconSheets_)
      {
         iconSheet.second->UpdateTextureInfo();
      }

      // Update OpenGL texture buffer data
      UpdateTextureBuffer();

      // Buffer texture data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * textureBuffer_.size(),
                      textureBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      lastTextureAtlasChanged_ = false;
   }

   // If buffers need updating
   if (dirty_)
   {
      // Buffer vertex data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(float) * currentIconBuffer_.size(),
                      currentIconBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      // Buffer threshold data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[2]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLint) * currentIntegerBuffer_.size(),
                      currentIntegerBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      numVertices_ =
         static_cast<GLsizei>(currentIconBuffer_.size() / kPointsPerVertex);
   }

   dirty_ = false;
}

bool GeoIcons::RunMousePicking(
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
         std::chrono::system_clock::now() :
         p->selectedTime_;

   // For each pickable icon
   auto it = std::find_if(
      std::execution::par_unseq,
      p->currentHoverIcons_.crbegin(),
      p->currentHoverIcons_.crend(),
      [&mapDistance, &selectedTime, &mapMatrix, &mouseCoords](const auto& icon)
      {
         if ((
                // Geo icon is thresholded
                mapDistance > units::length::meters<double> {0.0} &&

                // Geo icon threshold is < 999 nmi
                static_cast<int>(std::round(
                   units::length::nautical_miles<double> {icon.di_->threshold_}
                      .value())) < 999 &&

                // Map distance is beyond the threshold
                icon.di_->threshold_ < mapDistance) ||

             (
                // Geo icon has a start time
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
