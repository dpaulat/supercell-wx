#include <scwx/qt/map/radar_product_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/util/logger.hpp>

#include <execution>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <boost/algorithm/string.hpp>
#include <boost/timer/timer.hpp>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mbgl/util/constants.hpp>
#include <QGuiApplication>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace qt
{
namespace map
{

static constexpr uint32_t MAX_RADIALS           = 720;
static constexpr uint32_t MAX_DATA_MOMENT_GATES = 1840;

static const std::string logPrefix_ = "scwx::qt::map::radar_product_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class RadarProductLayerImpl
{
public:
   explicit RadarProductLayerImpl() :
       shaderProgram_(nullptr),
       uMVPMatrixLocation_(GL_INVALID_INDEX),
       uMapScreenCoordLocation_(GL_INVALID_INDEX),
       uDataMomentOffsetLocation_(GL_INVALID_INDEX),
       uDataMomentScaleLocation_(GL_INVALID_INDEX),
       uCFPEnabledLocation_(GL_INVALID_INDEX),
       vbo_ {GL_INVALID_INDEX},
       vao_ {GL_INVALID_INDEX},
       texture_ {GL_INVALID_INDEX},
       numVertices_ {0},
       cfpEnabled_ {false},
       colorTableNeedsUpdate_ {false},
       sweepNeedsUpdate_ {false}
   {
   }
   ~RadarProductLayerImpl() = default;

   std::shared_ptr<gl::ShaderProgram> shaderProgram_;

   GLint                 uMVPMatrixLocation_;
   GLint                 uMapScreenCoordLocation_;
   GLint                 uDataMomentOffsetLocation_;
   GLint                 uDataMomentScaleLocation_;
   GLint                 uCFPEnabledLocation_;
   std::array<GLuint, 3> vbo_;
   GLuint                vao_;
   GLuint                texture_;

   GLsizeiptr numVertices_;

   bool cfpEnabled_;

   bool colorTableNeedsUpdate_;
   bool sweepNeedsUpdate_;
};

RadarProductLayer::RadarProductLayer(std::shared_ptr<MapContext> context) :
    GenericLayer(context), p(std::make_unique<RadarProductLayerImpl>())
{
   auto radarProductView = context->radar_product_view();
   connect(radarProductView.get(),
           &view::RadarProductView::ColorTableLutUpdated,
           this,
           [this]() { p->colorTableNeedsUpdate_ = true; });
   connect(radarProductView.get(),
           &view::RadarProductView::SweepComputed,
           this,
           [this]() { p->sweepNeedsUpdate_ = true; });
}
RadarProductLayer::~RadarProductLayer() = default;

void RadarProductLayer::Initialize()
{
   logger_->debug("Initialize()");

   gl::OpenGLFunctions& gl = context()->gl();

   // Load and configure radar shader
   p->shaderProgram_ =
      context()->GetShaderProgram(":/gl/radar.vert", ":/gl/radar.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      logger_->warn("Could not find uMVPMatrix");
   }

   p->uMapScreenCoordLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uMapScreenCoord");
   if (p->uMapScreenCoordLocation_ == -1)
   {
      logger_->warn("Could not find uMapScreenCoord");
   }

   p->uDataMomentOffsetLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uDataMomentOffset");
   if (p->uDataMomentOffsetLocation_ == -1)
   {
      logger_->warn("Could not find uDataMomentOffset");
   }

   p->uDataMomentScaleLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uDataMomentScale");
   if (p->uDataMomentScaleLocation_ == -1)
   {
      logger_->warn("Could not find uDataMomentScale");
   }

   p->uCFPEnabledLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_->id(), "uCFPEnabled");
   if (p->uCFPEnabledLocation_ == -1)
   {
      logger_->warn("Could not find uCFPEnabled");
   }

   p->shaderProgram_->Use();

   // Generate a vertex array object
   gl.glGenVertexArrays(1, &p->vao_);

   // Generate vertex buffer objects
   gl.glGenBuffers(3, p->vbo_.data());

   // Update radar sweep
   p->sweepNeedsUpdate_ = true;
   UpdateSweep();

   // Create color table
   gl.glGenTextures(1, &p->texture_);
   p->colorTableNeedsUpdate_ = true;
   UpdateColorTable();
   gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   gl.glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

void RadarProductLayer::UpdateSweep()
{
   logger_->debug("UpdateSweep()");

   gl::OpenGLFunctions& gl = context()->gl();

   boost::timer::cpu_timer timer;

   std::shared_ptr<view::RadarProductView> radarProductView =
      context()->radar_product_view();

   std::unique_lock sweepLock(radarProductView->sweep_mutex(),
                              std::try_to_lock);
   if (!sweepLock.owns_lock())
   {
      logger_->debug("Sweep locked, deferring update");
      return;
   }

   p->sweepNeedsUpdate_ = false;

   const std::vector<float>& vertices = radarProductView->vertices();

   // Bind a vertex array object
   gl.glBindVertexArray(p->vao_);

   // Buffer vertices
   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   timer.start();
   gl.glBufferData(GL_ARRAY_BUFFER,
                   vertices.size() * sizeof(GLfloat),
                   vertices.data(),
                   GL_STATIC_DRAW);
   timer.stop();
   logger_->debug("Vertices buffered in {}", timer.format(6, "%ws"));

   gl.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(0);

   // Buffer data moments
   const GLvoid* data;
   GLsizeiptr    dataSize;
   size_t        componentSize;
   GLenum        type;

   std::tie(data, dataSize, componentSize) = radarProductView->GetMomentData();

   if (componentSize == 1)
   {
      type = GL_UNSIGNED_BYTE;
   }
   else
   {
      type = GL_UNSIGNED_SHORT;
   }

   gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   timer.start();
   gl.glBufferData(GL_ARRAY_BUFFER, dataSize, data, GL_STATIC_DRAW);
   timer.stop();
   logger_->debug("Data moments buffered in {}", timer.format(6, "%ws"));

   gl.glVertexAttribIPointer(1, 1, type, 0, static_cast<void*>(0));
   gl.glEnableVertexAttribArray(1);

   // Buffer CFP data
   const GLvoid* cfpData;
   GLsizeiptr    cfpDataSize;
   size_t        cfpComponentSize;
   GLenum        cfpType;

   std::tie(cfpData, cfpDataSize, cfpComponentSize) =
      radarProductView->GetCfpMomentData();

   if (cfpData != nullptr)
   {
      if (cfpComponentSize == 1)
      {
         cfpType = GL_UNSIGNED_BYTE;
      }
      else
      {
         cfpType = GL_UNSIGNED_SHORT;
      }

      gl.glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[2]);
      timer.start();
      gl.glBufferData(GL_ARRAY_BUFFER, cfpDataSize, cfpData, GL_STATIC_DRAW);
      timer.stop();
      logger_->debug("CFP moments buffered in {}", timer.format(6, "%ws"));

      gl.glVertexAttribIPointer(2, 1, cfpType, 0, static_cast<void*>(0));
      gl.glEnableVertexAttribArray(2);
   }
   else
   {
      gl.glDisableVertexAttribArray(2);
   }

   p->numVertices_ = vertices.size() / 2;
}

void RadarProductLayer::Render(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl();

   p->shaderProgram_->Use();

   // Set OpenGL blend mode for transparency
   gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   if (p->colorTableNeedsUpdate_)
   {
      UpdateColorTable();
   }

   if (p->sweepNeedsUpdate_)
   {
      UpdateSweep();
   }

   const float scale = std::pow(2.0, params.zoom) * 2.0f *
                       mbgl::util::tileSize_D / mbgl::util::DEGREES_MAX;
   const float xScale = scale / params.width;
   const float yScale = scale / params.height;

   glm::mat4 uMVPMatrix(1.0f);
   uMVPMatrix = glm::scale(uMVPMatrix, glm::vec3(xScale, yScale, 1.0f));
   uMVPMatrix = glm::rotate(uMVPMatrix,
                            glm::radians<float>(params.bearing),
                            glm::vec3(0.0f, 0.0f, 1.0f));

   gl.glUniform2fv(p->uMapScreenCoordLocation_,
                   1,
                   glm::value_ptr(util::maplibre::LatLongToScreenCoordinate(
                      {params.latitude, params.longitude})));

   gl.glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(uMVPMatrix));

   gl.glUniform1i(p->uCFPEnabledLocation_, p->cfpEnabled_ ? 1 : 0);

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_1D, p->texture_);
   gl.glBindVertexArray(p->vao_);
   gl.glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);

   SCWX_GL_CHECK_ERROR();
}

void RadarProductLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   gl::OpenGLFunctions& gl = context()->gl();

   gl.glDeleteVertexArrays(1, &p->vao_);
   gl.glDeleteBuffers(3, p->vbo_.data());

   p->uMVPMatrixLocation_        = GL_INVALID_INDEX;
   p->uMapScreenCoordLocation_   = GL_INVALID_INDEX;
   p->uDataMomentOffsetLocation_ = GL_INVALID_INDEX;
   p->uDataMomentScaleLocation_  = GL_INVALID_INDEX;
   p->uCFPEnabledLocation_       = GL_INVALID_INDEX;
   p->vao_                       = GL_INVALID_INDEX;
   p->vbo_                       = {GL_INVALID_INDEX};
   p->texture_                   = GL_INVALID_INDEX;
}

bool RadarProductLayer::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& mouseGlobalPos,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& mouseGeoCoords)
{
   bool itemPicked = false;

   if (QGuiApplication::keyboardModifiers() &
       Qt::KeyboardModifier::ShiftModifier)
   {
      std::shared_ptr<view::RadarProductView> radarProductView =
         context()->radar_product_view();

      std::optional<std::uint16_t> binLevel =
         radarProductView->GetBinLevel(mouseGeoCoords);

      if (binLevel.has_value())
      {
         // Hovering over a bin on the map
         std::optional<wsr88d::DataLevelCode> code =
            radarProductView->GetDataLevelCode(binLevel.value());
         std::optional<float> value =
            radarProductView->GetDataValue(binLevel.value());

         if (code.has_value() && //
             code.value() != wsr88d::DataLevelCode::Blank &&
             code.value() != wsr88d::DataLevelCode::NoData)
         {
            // Level has associated data level code
            std::string codeName = wsr88d::GetDataLevelCodeName(code.value());
            std::string codeShortName =
               wsr88d::GetDataLevelCodeShortName(code.value());
            std::string hoverText;

            if (codeName != codeShortName && !codeShortName.empty())
            {
               // There is a unique long and short name for the code
               hoverText = fmt::format("{}: {}", codeShortName, codeName);
            }
            else
            {
               // Otherwise, only use the long name (always present)
               hoverText = codeName;
            }

            // Show the tooltip
            util::tooltip::Show(hoverText, mouseGlobalPos);

            itemPicked = true;
         }
         else if (value.has_value())
         {
            // Level has associated data value
            float       f = value.value();
            std::string units {};
            std::string hoverText;

            std::shared_ptr<common::ColorTable> colorTable =
               radarProductView->color_table();

            if (colorTable != nullptr)
            {
               // Scale data value according to the color table, and get units
               f     = f * colorTable->scale() + colorTable->offset();
               units = colorTable->units();
            }

            if (units.empty() ||          //
                units.starts_with("?") || //
                boost::iequals(units, "NONE") ||
                boost::iequals(units, "UNITLESS"))
            {
               // Don't display a units value that wasn't intended to be
               // displayed
               hoverText = fmt::format("{}", f);
            }
            else if (std::isalpha(units.at(0)))
            {
               // dBZ, Kts, etc.
               hoverText = fmt::format("{} {}", f, units);
            }
            else
            {
               // %, etc.
               hoverText = fmt::format("{}{}", f, units);
            }

            // Show the tooltip
            util::tooltip::Show(hoverText, mouseGlobalPos);

            itemPicked = true;
         }
      }
   }

   return itemPicked;
}

void RadarProductLayer::UpdateColorTable()
{
   logger_->debug("UpdateColorTable()");

   p->colorTableNeedsUpdate_ = false;

   gl::OpenGLFunctions&                    gl = context()->gl();
   std::shared_ptr<view::RadarProductView> radarProductView =
      context()->radar_product_view();

   const std::vector<boost::gil::rgba8_pixel_t>& colorTable =
      radarProductView->color_table_lut();
   const uint16_t rangeMin = radarProductView->color_table_min();
   const uint16_t rangeMax = radarProductView->color_table_max();

   const float scale = rangeMax - rangeMin;

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_1D, p->texture_);
   gl.glTexImage1D(GL_TEXTURE_1D,
                   0,
                   GL_RGBA,
                   (GLsizei) colorTable.size(),
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   colorTable.data());
   gl.glGenerateMipmap(GL_TEXTURE_1D);

   gl.glUniform1ui(p->uDataMomentOffsetLocation_, rangeMin);
   gl.glUniform1f(p->uDataMomentScaleLocation_, scale);
}

} // namespace map
} // namespace qt
} // namespace scwx
