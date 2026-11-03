#include <scwx/qt/gl/draw/placefile_packed_vertex.hpp>
#include <scwx/qt/gl/draw/placefile_triangles.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

#include <mutex>

namespace scwx
{
namespace qt
{
namespace gl
{
namespace draw
{

static const std::string logPrefix_ = "scwx::qt::gl::draw::placefile_triangles";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::size_t kVerticesPerTriangle = 3;

class PlacefileTriangles::Impl
{
public:
   explicit Impl(const std::shared_ptr<GlContext>& context) :
       context_ {context},
       shaderProgram_ {nullptr},
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       uMapDistanceLocation_(GL_INVALID_INDEX),
       uSelectedTimeLocation_(GL_INVALID_INDEX),
       numVertices_ {0}
   {
   }

   ~Impl() {}

   void UpdateBuffers(
      const std::shared_ptr<const gr::Placefile::TrianglesDrawItem>& di);
   void Update();

   std::shared_ptr<GlContext> context_;

   bool dirty_ {false};
   bool thresholded_ {false};

   std::chrono::system_clock::time_point selectedTime_ {};

   std::mutex bufferMutex_ {};

   SharedBuffer<PackedFillVertex> currentBuffer_ {};
   std::vector<PackedFillVertex>  newBuffer_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;
   GLint                          uSelectedTimeLocation_;

   GLuint vao_ {GL_INVALID_INDEX};
   GLuint vbo_ {GL_INVALID_INDEX};

   GLsizei numVertices_;
};

PlacefileTriangles::PlacefileTriangles(
   const std::shared_ptr<GlContext>& context) :
    DrawItem(), p(std::make_unique<Impl>(context))
{
}
PlacefileTriangles::~PlacefileTriangles() = default;

PlacefileTriangles::PlacefileTriangles(PlacefileTriangles&&) noexcept = default;
PlacefileTriangles&
PlacefileTriangles::operator=(PlacefileTriangles&&) noexcept = default;

void PlacefileTriangles::set_selected_time(
   std::chrono::system_clock::time_point selectedTime)
{
   p->selectedTime_ = selectedTime;
}

void PlacefileTriangles::set_thresholded(bool thresholded)
{
   p->thresholded_ = thresholded;
}

void PlacefileTriangles::Initialize()
{
   p->shaderProgram_ = p->context_->GetShaderProgram(
      {{GL_VERTEX_SHADER, ":/gl/map_color.vert"},
       {GL_GEOMETRY_SHADER, ":/gl/threshold.geom"},
       {GL_FRAGMENT_SHADER, ":/gl/color.frag"}});

   p->uMVPMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMVPMatrix");
   p->uMapMatrixLocation_ = p->shaderProgram_->GetUniformLocation("uMapMatrix");
   p->uMapScreenCoordLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapScreenCoord");
   p->uMapDistanceLocation_ =
      p->shaderProgram_->GetUniformLocation("uMapDistance");
   p->uSelectedTimeLocation_ =
      p->shaderProgram_->GetUniformLocation("uSelectedTime");

   glGenVertexArrays(1, &p->vao_);
   glGenBuffers(1, &p->vbo_);

   glBindVertexArray(p->vao_);
   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // NOLINTBEGIN(modernize-use-nullptr)
   // NOLINTBEGIN(performance-no-int-to-ptr)
   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

   constexpr GLsizei kStride = sizeof(PackedFillVertex);

   // aScreenCoord
   glVertexAttribPointer(
      0,
      2,
      GL_FLOAT,
      GL_FALSE,
      kStride,
      reinterpret_cast<void*>(offsetof(PackedFillVertex, screenX_)));
   glEnableVertexAttribArray(0);

   // aXYOffset
   glVertexAttribPointer(
      1,
      2,
      GL_HALF_FLOAT,
      GL_FALSE,
      kStride,
      reinterpret_cast<void*>(offsetof(PackedFillVertex, xOffsetHalf_)));
   glEnableVertexAttribArray(1);

   // aColor
   glVertexAttribPointer(
      2,
      4,
      GL_UNSIGNED_BYTE,
      GL_TRUE,
      kStride,
      reinterpret_cast<void*>(offsetof(PackedFillVertex, rgba_)));
   glEnableVertexAttribArray(2);

   // aThreshold
   glVertexAttribIPointer(
      3, //
      1,
      GL_SHORT,
      kStride,
      reinterpret_cast<void*>(offsetof(PackedFillVertex, threshold_)));
   glEnableVertexAttribArray(3);

   // aTimeRange
   glVertexAttribIPointer(
      4, //
      2,
      GL_INT,
      kStride,
      reinterpret_cast<void*>(offsetof(PackedFillVertex, startTime_)));
   glEnableVertexAttribArray(4);

   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
   // NOLINTEND(performance-no-int-to-ptr)
   // NOLINTEND(modernize-use-nullptr)

   p->dirty_ = true;
}

void PlacefileTriangles::Render(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   bool hasGeometry = false;
   {
      const std::unique_lock lock {p->bufferMutex_};
      hasGeometry = p->currentBuffer_ != nullptr && !p->currentBuffer_->empty();
   }

   if (hasGeometry)
   {
      glBindVertexArray(p->vao_);

      p->Update();
      p->shaderProgram_->Use();
      UseRotationProjection(params, p->uMVPMatrixLocation_);
      UseMapScreenProjection(
         params, p->uMapMatrixLocation_, p->uMapScreenCoordLocation_);

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
      glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefileTriangles::Deinitialize()
{
   glDeleteVertexArrays(1, &p->vao_);
   glDeleteBuffers(1, &p->vbo_);

   const std::unique_lock lock {p->bufferMutex_};

   // Clear the current buffers
   p->currentBuffer_.reset();
}

void PlacefileTriangles::StartTriangles()
{
   // Clear the new buffers
   p->newBuffer_.clear();
}

void PlacefileTriangles::AddTriangles(
   const std::shared_ptr<gr::Placefile::TrianglesDrawItem>& di)
{
   if (di != nullptr)
   {
      p->UpdateBuffers(di);
   }
}

void PlacefileTriangles::FinishTriangles()
{
   const std::unique_lock lock {p->bufferMutex_};

   // Swap buffers
   p->currentBuffer_ = InternBuffer(std::move(p->newBuffer_));

   // Clear the new buffer
   p->newBuffer_.clear();

   // Mark the draw item dirty
   p->dirty_ = true;
}

void PlacefileTriangles::Impl::UpdateBuffers(
   const std::shared_ptr<const gr::Placefile::TrianglesDrawItem>& di)
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

   // Default color to "Color" statement
   boost::gil::rgba8_pixel_t lastColor = di->color_;

   // For each element inside a Triangles statement, add a vertex
   for (auto& element : di->elements_)
   {
      // Calculate screen coordinate
      auto screenCoordinate = util::maplibre::LatLongToScreenCoordinate(
         {element.latitude_, element.longitude_});

      // X/Y offset in pixels
      const float x = static_cast<float>(element.x_);
      const float y = static_cast<float>(element.y_);

      // Update the most recent color if specified
      if (element.color_.has_value())
      {
         lastColor = element.color_.value();
      }

      newBuffer_.push_back(PackedFillVertex {
         .screenX_     = screenCoordinate.x,
         .screenY_     = screenCoordinate.y,
         .xOffsetHalf_ = FloatToHalfBits(x),
         .yOffsetHalf_ = FloatToHalfBits(y),
         .rgba_      = {lastColor[0], lastColor[1], lastColor[2], lastColor[3]},
         .threshold_ = ClampThreshold(thresholdValue),
         .padding_   = 0,
         .startTime_ = startTime,
         .endTime_   = endTime});
   }

   // Remove extra vertices that don't correspond to a full triangle
   newBuffer_.resize(newBuffer_.size() -
                     (newBuffer_.size() % kVerticesPerTriangle));
}

void PlacefileTriangles::Impl::Update()
{
   if (dirty_)
   {
      std::unique_lock lock {bufferMutex_};

      // Buffer vertex data
      glBindBuffer(GL_ARRAY_BUFFER, vbo_);
      if (currentBuffer_ != nullptr && !currentBuffer_->empty())
      {
         glBufferData(GL_ARRAY_BUFFER,
                      static_cast<GLsizeiptr>(sizeof(PackedFillVertex) *
                                              currentBuffer_->size()),
                      currentBuffer_->data(),
                      GL_DYNAMIC_DRAW);
         numVertices_ = static_cast<GLsizei>(currentBuffer_->size());
      }
      else
      {
         glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);
         numVertices_ = 0;
      }

      dirty_ = false;
   }
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
