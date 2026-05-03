#include <scwx/qt/map/color_table_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/qt/view/radar_product_view.hpp>
#include <scwx/util/logger.hpp>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::color_table_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ColorTableLayer::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   std::shared_ptr<gl::ShaderProgram> shaderProgram_ {nullptr};

   GLint uMVPMatrixLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   std::array<GLuint, 2> vbo_ {GL_INVALID_INDEX};
   GLuint                vao_ {GL_INVALID_INDEX};
   GLuint                texture_ {GL_INVALID_INDEX};

   std::vector<boost::gil::rgba8_pixel_t> colorTable_ {};

   bool colorTableNeedsUpdate_ {true};
};

ColorTableLayer::ColorTableLayer(std::shared_ptr<gl::GlContext> glContext) :
    GenericLayer(std::move(glContext)), p(std::make_unique<Impl>())
{
}
ColorTableLayer::~ColorTableLayer() = default;

void ColorTableLayer::Initialize(const std::shared_ptr<MapContext>& mapContext)
{
   logger_->debug("Initialize()");

   auto glContext = gl_context();

   // Load and configure overlay shader
   p->shaderProgram_ =
      glContext->GetShaderProgram(":/gl/texture1d.vert", ":/gl/texture1d.frag");

   p->uMVPMatrixLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      logger_->warn("Could not find uMVPMatrix");
   }

   glGenTextures(1, &p->texture_);

   p->shaderProgram_->Use();

   // Generate a vertex array object
   glGenVertexArrays(1, &p->vao_);

   // Generate vertex buffer objects
   glGenBuffers(2, p->vbo_.data());

   glBindVertexArray(p->vao_);

   // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   // NOLINTBEGIN(modernize-use-nullptr)

   // Bottom panel
   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
   glBufferData(
      GL_ARRAY_BUFFER, sizeof(float) * 6 * 2, nullptr, GL_DYNAMIC_DRAW);

   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   glEnableVertexAttribArray(0);

   // Color table panel texture coordinates
   // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
   const float textureCoords[6][1] = {{0.0f}, // TL
                                      {0.0f}, // BL
                                      {1.0f}, // TR
                                      //
                                      {0.0f},  // BL
                                      {1.0f},  // TR
                                      {1.0f}}; // BR
   glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[1]);
   glBufferData(GL_ARRAY_BUFFER,
                sizeof(textureCoords),
                static_cast<const void*>(textureCoords),
                GL_STATIC_DRAW);

   glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, static_cast<void*>(0));
   glEnableVertexAttribArray(1);

   // NOLINTEND(modernize-use-nullptr)
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

   connect(mapContext->radar_product_view().get(),
           &view::RadarProductView::ColorTableLutUpdated,
           this,
           [this]() { p->colorTableNeedsUpdate_ = true; });
}

void ColorTableLayer::Render(
   const std::shared_ptr<MapContext>&            mapContext,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   auto radarProductView = mapContext->radar_product_view();

   if (radarProductView == nullptr || !radarProductView->IsInitialized())
   {
      // No color bar: clear reserved bottom margin (overlay uses this for
      // attribution / logo). Avoid stale margin when the table is hidden.
      mapContext->set_color_table_margins({});
      return;
   }

   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   p->shaderProgram_->Use();

   // Set OpenGL blend mode for transparency
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(projection));

   if (p->colorTableNeedsUpdate_)
   {
      p->colorTable_ = radarProductView->color_table_lut();

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_1D, p->texture_);
      glTexImage1D(GL_TEXTURE_1D,
                   0,
                   GL_RGBA,
                   (GLsizei) p->colorTable_.size(),
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   p->colorTable_.data());
      glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glGenerateMipmap(GL_TEXTURE_1D);
   }

   if (p->colorTable_.size() > 0 && radarProductView->sweep_time() !=
                                       std::chrono::system_clock::time_point())
   {
      // NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
      // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

      // Color table panel vertices
      const float vertexLX       = 0.0f;
      const float vertexRX       = static_cast<float>(params.width);
      const float vertexTY       = 10.0f;
      const float vertexBY       = 0.0f;
      const float vertices[6][2] = {{vertexLX, vertexTY}, // TL
                                    {vertexLX, vertexBY}, // BL
                                    {vertexRX, vertexTY}, // TR
                                    //
                                    {vertexLX, vertexBY},  // BL
                                    {vertexRX, vertexTY},  // TR
                                    {vertexRX, vertexBY}}; // BR

      // Draw vertices
      glBindVertexArray(p->vao_);
      glBindBuffer(GL_ARRAY_BUFFER, p->vbo_[0]);
      glBufferSubData(GL_ARRAY_BUFFER,
                      0,
                      sizeof(vertices),
                      static_cast<const void*>(vertices));
      glDrawArrays(GL_TRIANGLES, 0, 6);

      static constexpr int kLeftMargin_   = 0;
      static constexpr int kTopMargin_    = 0;
      static constexpr int kRightMargin_  = 0;
      static constexpr int kBottomMargin_ = 10;

      mapContext->set_color_table_margins(
         QMargins {kLeftMargin_, kTopMargin_, kRightMargin_, kBottomMargin_});

      // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
      // NOLINTEND(cppcoreguidelines-avoid-c-arrays)
   }
   else
   {
      mapContext->set_color_table_margins(QMargins {});
   }

   SCWX_GL_CHECK_ERROR();
}

void ColorTableLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   glDeleteVertexArrays(1, &p->vao_);
   glDeleteBuffers(2, p->vbo_.data());
   glDeleteTextures(1, &p->texture_);

   p->uMVPMatrixLocation_ = GL_INVALID_INDEX;
   p->vao_                = GL_INVALID_INDEX;
   p->vbo_                = {GL_INVALID_INDEX};
   p->texture_            = GL_INVALID_INDEX;
}

} // namespace scwx::qt::map
