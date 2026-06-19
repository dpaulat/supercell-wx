#include <scwx/qt/gl/draw/geo_lines.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <execution>

#include <boost/unordered/unordered_flat_set.hpp>
#include <units/angle.h>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::geo_lines";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr size_t kNumRectangles        = 1;
static constexpr size_t kNumTriangles         = kNumRectangles * 2;
static constexpr size_t kVerticesPerTriangle  = 3;
static constexpr size_t kVerticesPerRectangle = kVerticesPerTriangle * 2;
static constexpr size_t kPointsPerVertex      = 9;
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
   float                        latitude1_ {};
   float                        longitude1_ {};
   float                        latitude2_ {};
   float                        longitude2_ {};
   float                        width_ {5.0};
   units::angle::degrees<float> angle_ {};
   std::string                  hoverText_ {};
   GeoLines::HoverCallback      hoverCallback_ {nullptr};
   size_t                       lineIndex_ {0};
};

class GeoLines::Impl
{
public:
   struct LineHoverEntry
   {
      std::shared_ptr<GeoLineDrawItem> di_;

      glm::vec2 p1_;
      glm::vec2 p2_;
      glm::vec2 otl_;
      glm::vec2 otr_;
      glm::vec2 obl_;
      glm::vec2 obr_;
   };

   explicit Impl(std::shared_ptr<GlContext> context) :
       context_ {context},
       shaderProgram_ {nullptr},
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX}
   {
   }

   ~Impl() {}

   void BufferLine(const std::shared_ptr<const GeoLineDrawItem>& di);
   void Update();
   void UpdateBuffers();
   void UpdateModifiedLineBuffers();
   void UpdateSingleBuffer(const std::shared_ptr<GeoLineDrawItem>& di,
                           std::vector<float>&                     linesBuffer,
                           std::vector<GLint>&                 integerBuffer,
                           std::unordered_map<std::shared_ptr<GeoLineDrawItem>,
                                              LineHoverEntry>& hoverLines);

   std::shared_ptr<GlContext> context_;

   bool visible_ {true};
   bool dirty_ {false};
   bool thresholded_ {false};

   boost::unordered_flat_set<std::shared_ptr<GeoLineDrawItem>> dirtyLines_ {};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex lineMutex_ {};

   std::vector<std::shared_ptr<GeoLineDrawItem>> currentLineList_ {};
   std::vector<std::shared_ptr<GeoLineDrawItem>> newLineList_ {};

   std::vector<float> currentLinesBuffer_ {};
   std::vector<GLint> currentIntegerBuffer_ {};
   std::vector<float> newLinesBuffer_ {};
   std::vector<GLint> newIntegerBuffer_ {};

   std::unordered_map<std::shared_ptr<GeoLineDrawItem>, LineHoverEntry>
      currentHoverLines_ {};
   std::unordered_map<std::shared_ptr<GeoLineDrawItem>, LineHoverEntry>
      newHoverLines_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;

   GLint uMVPMatrixLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uMapMatrixLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uOriginLatLongLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uMapDistanceLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uSelectedTimeLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;
};

GeoLines::GeoLines(std::shared_ptr<GlContext> context) :
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
   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/geo_texture2d.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/color.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");
   p->uMapMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMapMatrix");
   p->uOriginLatLongLocation_ =
      p->shaderProgram_->GetUniformLocation("uOriginLatLong");
   p->uMapDistanceLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapDistance");
   p->uSelectedTimeLocation_ =
      p->shaderProgram_->GetUniformLocation("uSelectedTime");

   glGenVertexArrays(1, &p->vao_);
   glGenBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   glBindVertexArray(p->vao_);
   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   glBufferData(GL_ARRAY_BUFFER,
                sizeof(float) * kLineBufferLength_,
                nullptr,
                GL_DYNAMIC_DRAW);

   // NOLINTBEGIN(modernize-use-nullptr)
   // NOLINTBEGIN(performance-no-int-to-ptr)
   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

   // aLatLong
   glVertexAttribPointer(0,
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerVertex * sizeof(float),
                         static_cast<void*>(0));
   glEnableVertexAttribArray(0);

   // aXYOffset
   glVertexAttribPointer(1,
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(2 * sizeof(float)));
   glEnableVertexAttribArray(1);

   // aModulate
   glVertexAttribPointer(3,
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(4 * sizeof(float)));
   glEnableVertexAttribArray(3);

   // aAngle
   glVertexAttribPointer(4,
                         1,
                         GL_FLOAT,
                         GL_FALSE,
                         kPointsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(8 * sizeof(float)));
   glEnableVertexAttribArray(4);

   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aThreshold
   glVertexAttribIPointer(5, //
                          1,
                          GL_INT,
                          kIntegersPerVertex_ * sizeof(GLint),
                          static_cast<void*>(0));
   glEnableVertexAttribArray(5);

   // aTimeRange
   glVertexAttribIPointer(6, //
                          2,
                          GL_INT,
                          kIntegersPerVertex_ * sizeof(GLint),
                          reinterpret_cast<void*>(1 * sizeof(GLint)));
   glEnableVertexAttribArray(6);

   // aDisplayed
   glVertexAttribIPointer(7,
                          1,
                          GL_INT,
                          kIntegersPerVertex_ * sizeof(GLint),
                          reinterpret_cast<void*>(3 * sizeof(GLint)));
   glEnableVertexAttribArray(7);

   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
   // NOLINTEND(performance-no-int-to-ptr)
   // NOLINTEND(modernize-use-nullptr)

   p->dirty_ = true;
}

void GeoLines::Render(const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->visible_)
   {
      return;
   }

   std::unique_lock lock {p->lineMutex_};

   if (p->newLineList_.size() > 0)
   {
      glBindVertexArray(p->vao_);

      p->Update();
      p->shaderProgram_->Use();
      UseRotationProjection(params, p->uMVPMatrixLocation_);
      UseMapProjection(
         params, p->uMapMatrixLocation_, p->uOriginLatLongLocation_);

      if (p->thresholded_)
      {
         // If thresholding is enabled, set the map distance
         units::length::nautical_miles<float> mapDistance =
            util::maplibre::GetMapDistance(params);
         glUniform1f(p->uMapDistanceLocation_, mapDistance.value());
      }
      else
      {
         // If thresholding is disabled, set the map distance to 0
         glUniform1f(p->uMapDistanceLocation_, 0.0f);
      }

      // Selected time
      std::chrono::system_clock::time_point selectedTime =
         (p->selectedTime_ == std::chrono::system_clock::time_point {}) ?
            scwx::util::time::now() :
            p->selectedTime_;
      glUniform1i(
         p->uSelectedTimeLocation_,
         static_cast<GLint>(std::chrono::duration_cast<std::chrono::minutes>(
                               selectedTime.time_since_epoch())
                               .count()));

      // Draw icons
      glDrawArrays(GL_TRIANGLES,
                   0,
                   static_cast<GLsizei>(p->currentLineList_.size() *
                                        kVerticesPerRectangle));
   }
}

void GeoLines::Deinitialize()
{
   glDeleteVertexArrays(1, &p->vao_);
   glDeleteBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());

   std::unique_lock lock {p->lineMutex_};

   p->currentLinesBuffer_.clear();
   p->currentIntegerBuffer_.clear();
   p->currentHoverLines_.clear();
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
       di->latitude2_ != latitude1 || di->longitude2_ != longitude1)
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
   if (di->width_ != width)
   {
      di->width_ = width;
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

void GeoLines::FinishLines()
{
   // Update buffers
   p->UpdateBuffers();

   std::unique_lock lock {p->lineMutex_};

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
      dirty_ = true;
   }
}

void GeoLines::Impl::UpdateSingleBuffer(
   const std::shared_ptr<GeoLineDrawItem>& di,
   std::vector<float>&                     lineBuffer,
   std::vector<GLint>&                     integerBuffer,
   std::unordered_map<std::shared_ptr<GeoLineDrawItem>, LineHoverEntry>&
      hoverLines)
{
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
   const float hw = di->width_ * 0.5f;
   const float lx = -hw;
   const float rx = +hw;
   const float ty = +hw;
   const float by = -hw;

   // Modulate color
   const float mc0 = di->modulate_[0];
   const float mc1 = di->modulate_[1];
   const float mc2 = di->modulate_[2];
   const float mc3 = di->modulate_[3];

   // Visibility
   const GLint v = static_cast<GLint>(di->visible_);

   // Initiailize line data
   const auto lineData = {
      // Line
      lat1, lon1, lx, by, mc0, mc1, mc2, mc3, a, // BL
      lat2, lon2, lx, ty, mc0, mc1, mc2, mc3, a, // TL
      lat1, lon1, rx, by, mc0, mc1, mc2, mc3, a, // BR
      lat1, lon1, rx, by, mc0, mc1, mc2, mc3, a, // BR
      lat2, lon2, rx, ty, mc0, mc1, mc2, mc3, a, // TR
      lat2, lon2, lx, ty, mc0, mc1, mc2, mc3, a  // TL
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

      const glm::vec2 otl = rotate * glm::vec2 {-hw, +hw};
      const glm::vec2 otr = rotate * glm::vec2 {+hw, +hw};
      const glm::vec2 obl = rotate * glm::vec2 {-hw, -hw};
      const glm::vec2 obr = rotate * glm::vec2 {+hw, -hw};

      if (hoverIt == hoverLines.end())
      {
         hoverLines.emplace(di,
                            LineHoverEntry {.di_  = di,
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

   // If the lines have been updated
   if (dirty_)
   {
      // Buffer lines data
      glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      glBufferData(
         GL_ARRAY_BUFFER,
         static_cast<GLsizeiptr>(sizeof(float) * currentLinesBuffer_.size()),
         currentLinesBuffer_.data(),
         GL_DYNAMIC_DRAW);

      // Buffer threshold data
      glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      glBufferData(
         GL_ARRAY_BUFFER,
         static_cast<GLsizeiptr>(sizeof(GLint) * currentIntegerBuffer_.size()),
         currentIntegerBuffer_.data(),
         GL_DYNAMIC_DRAW);
   }

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

   // For each pickable line
   auto it = std::find_if(
      std::execution::par_unseq,
      p->currentHoverLines_.cbegin(),
      p->currentHoverLines_.cend(),
      [&mapDistance, &selectedTime, &mapMatrix, &mouseCoords](
         const auto& lineIt)
      {
         const auto& line = lineIt.second;
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

   if (it != p->currentHoverLines_.cend())
   {
      itemPicked = true;

      if (!it->second.di_->hoverText_.empty())
      {
         // Show tooltip
         util::tooltip::Show(it->second.di_->hoverText_, mouseGlobalPos);
      }
      else if (it->second.di_->hoverCallback_ != nullptr)
      {
         it->second.di_->hoverCallback_(it->second.di_, mouseGlobalPos);
      }

      if (it->second.di_->event_ != nullptr)
      {
         // Register event handler
         eventHandler = it->second.di_;
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
} // namespace gl
} // namespace qt
} // namespace scwx
