#include <scwx/qt/gl/draw/placefile_triangles.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

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
static constexpr std::size_t kPointsPerVertex     = 8;

// Threshold, start time, end time
static constexpr std::size_t kIntegersPerVertex_ = 3;

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
       vao_ {GL_INVALID_INDEX},
       vbo_ {GL_INVALID_INDEX},
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

   std::vector<GLfloat> currentBuffer_ {};
   std::vector<GLint>   currentIntegerBuffer_ {};
   std::vector<GLfloat> newBuffer_ {};
   std::vector<GLint>   newIntegerBuffer_ {};

   std::shared_ptr<ShaderProgram> shaderProgram_;
   GLint                          uMVPMatrixLocation_;
   GLint                          uMapMatrixLocation_;
   GLint                          uMapScreenCoordLocation_;
   GLint                          uMapDistanceLocation_;
   GLint                          uSelectedTimeLocation_;

   GLuint                vao_;
   std::array<GLuint, 2> vbo_;

   GLsizei numVertices_;
};

PlacefileTriangles::PlacefileTriangles(
   const std::shared_ptr<GlContext>& context) :
    DrawItem(context->gl()), p(std::make_unique<Impl>(context))
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
   gl::OpenGLFunctions& gl = p->context_->gl();

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

   gl.glGenVertexArrays(1, &p->vao_);
   gl.glGenBuffers(2, p->vbo_.data());

   gl.glBindVertexArray(p->vao_);
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aScreenCoord
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

   // aColor
   gl.glVertexAttribPointer(2,
                            4,
                            GL_FLOAT,
                            GL_FALSE,
                            kPointsPerVertex * sizeof(float),
                            reinterpret_cast<void*>(4 * sizeof(float)));
   gl.glEnableVertexAttribArray(2);

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   gl.glBufferData(GL_ARRAY_BUFFER, 0u, nullptr, GL_DYNAMIC_DRAW);

   // aThreshold
   gl.glVertexAttribIPointer(3, //
                             1,
                             GL_INT,
                             kIntegersPerVertex_ * sizeof(GLint),
                             static_cast<void*>(0));
   gl.glEnableVertexAttribArray(3);

   // aTimeRange
   gl.glVertexAttribIPointer(4, //
                             2,
                             GL_INT,
                             kIntegersPerVertex_ * sizeof(GLint),
                             reinterpret_cast<void*>(1 * sizeof(GLint)));
   gl.glEnableVertexAttribArray(4);

   p->dirty_ = true;
}

void PlacefileTriangles::Render(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->currentBuffer_.empty())
   {
      gl::OpenGLFunctions& gl = p->context_->gl();

      gl.glBindVertexArray(p->vao_);

      p->Update();
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

      // Draw icons
      gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);
   }
}

void PlacefileTriangles::Deinitialize()
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(2, p->vbo_.data());

   std::unique_lock lock {p->bufferMutex_};

   // Clear the current buffers
   p->currentBuffer_.clear();
   p->currentIntegerBuffer_.clear();
}

void PlacefileTriangles::StartTriangles()
{
   // Clear the new buffers
   p->newBuffer_.clear();
   p->newIntegerBuffer_.clear();
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
   std::unique_lock lock {p->bufferMutex_};

   // Swap buffers
   p->currentBuffer_.swap(p->newBuffer_);
   p->currentIntegerBuffer_.swap(p->newIntegerBuffer_);

   // Clear the new buffers
   p->newBuffer_.clear();
   p->newIntegerBuffer_.clear();

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

      // Color value
      const float r = lastColor[0] / 255.0f;
      const float g = lastColor[1] / 255.0f;
      const float b = lastColor[2] / 255.0f;
      const float a = lastColor[3] / 255.0f;

      newBuffer_.insert(
         newBuffer_.end(),
         {screenCoordinate.x, screenCoordinate.y, x, y, r, g, b, a});
      newIntegerBuffer_.insert(newIntegerBuffer_.end(),
                               {thresholdValue, startTime, endTime});
   }

   // Remove extra vertices that don't correspond to a full triangle
   while (newBuffer_.size() % kVerticesPerTriangle != 0)
   {
      newBuffer_.pop_back();
      newIntegerBuffer_.pop_back();
   }
}

void PlacefileTriangles::Impl::Update()
{
   if (dirty_)
   {
      gl::OpenGLFunctions& gl = context_->gl();

      std::unique_lock lock {bufferMutex_};

      // Buffer vertex data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLfloat) * currentBuffer_.size(),
                      currentBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      // Buffer threshold data
      gl.glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
      gl.glBufferData(GL_ARRAY_BUFFER,
                      sizeof(GLint) * currentIntegerBuffer_.size(),
                      currentIntegerBuffer_.data(),
                      GL_DYNAMIC_DRAW);

      numVertices_ =
         static_cast<GLsizei>(currentBuffer_.size() / kPointsPerVertex);

      dirty_ = false;
   }
}

} // namespace draw
} // namespace gl
} // namespace qt
} // namespace scwx
