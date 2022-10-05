#include <scwx/qt/gl/gl_context.hpp>
#include <scwx/qt/util/streams.hpp>
#include <scwx/util/hash.hpp>
#include <scwx/util/logger.hpp>

#pragma warning(push, 0)
#pragma warning(disable : 4714)
#include <boost/gil.hpp>
#include <boost/gil/extension/io/png.hpp>
#include <boost/iostreams/stream.hpp>
#include <QFile>
#pragma warning(pop)

namespace scwx
{
namespace qt
{
namespace gl
{

static const std::string logPrefix_ = "scwx::qt::gl::gl_context";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class GlContext::Impl
{
public:
   explicit Impl() :
       gl_ {},
       shaderProgramMap_ {},
       shaderProgramMutex_ {},
       textureMap_ {},
       textureMutex_ {}
   {
   }
   ~Impl() {}

   GLuint CreateTexture(const std::string& texturePath);

   gl::OpenGLFunctions gl_;

   std::unordered_map<std::pair<std::string, std::string>,
                      std::shared_ptr<gl::ShaderProgram>,
                      scwx::util::hash<std::pair<std::string, std::string>>>
              shaderProgramMap_;
   std::mutex shaderProgramMutex_;

   std::unordered_map<std::string, GLuint> textureMap_;
   std::mutex                              textureMutex_;
};

GlContext::GlContext() : p(std::make_unique<Impl>()) {}
GlContext::~GlContext() = default;

GlContext::GlContext(GlContext&&) noexcept            = default;
GlContext& GlContext::operator=(GlContext&&) noexcept = default;

gl::OpenGLFunctions& GlContext::gl()
{
   return p->gl_;
}

std::shared_ptr<gl::ShaderProgram>
GlContext::GetShaderProgram(const std::string& vertexPath,
                            const std::string& fragmentPath)
{
   const std::pair<std::string, std::string> key {vertexPath, fragmentPath};
   std::shared_ptr<gl::ShaderProgram>        shaderProgram;

   std::unique_lock lock(p->shaderProgramMutex_);

   auto it = p->shaderProgramMap_.find(key);

   if (it == p->shaderProgramMap_.end())
   {
      shaderProgram = std::make_shared<gl::ShaderProgram>(p->gl_);
      shaderProgram->Load(vertexPath, fragmentPath);
      p->shaderProgramMap_[key] = shaderProgram;
   }
   else
   {
      shaderProgram = it->second;
   }

   return shaderProgram;
}

GLuint GlContext::GetTexture(const std::string& texturePath)
{
   GLuint texture = GL_INVALID_INDEX;

   std::unique_lock lock(p->textureMutex_);

   auto it = p->textureMap_.find(texturePath);

   if (it == p->textureMap_.end())
   {
      texture                     = p->CreateTexture(texturePath);
      p->textureMap_[texturePath] = texture;
   }
   else
   {
      texture = it->second;
   }

   return texture;
}

// TODO: Move to dedicated file
GLuint GlContext::Impl::CreateTexture(const std::string& texturePath)
{
   logger_->warn("Create Texture: {}", texturePath);

   GLuint texture;

   QFile textureFile(texturePath.c_str());

   textureFile.open(QIODevice::ReadOnly);

   if (!textureFile.isOpen())
   {
      logger_->error("Could not load texture: {}", texturePath);
      return GL_INVALID_INDEX;
   }

   boost::iostreams::stream<util::IoDeviceSource> dataStream(textureFile);

   boost::gil::rgba8_image_t image;

   try
   {
      boost::gil::read_and_convert_image(
         dataStream, image, boost::gil::png_tag());
   }
   catch (const std::exception& ex)
   {
      logger_->error("Error reading texture: {}", ex.what());
      return GL_INVALID_INDEX;
   }

   boost::gil::rgba8_view_t view = boost::gil::view(image);

   std::vector<boost::gil::rgba8_pixel_t> pixelData(view.width() *
                                                    view.height());

   boost::gil::copy_pixels(
      view,
      boost::gil::interleaved_view(view.width(),
                                   view.height(),
                                   pixelData.data(),
                                   view.width() *
                                      sizeof(boost::gil::rgba8_pixel_t)));

   OpenGLFunctions& gl = gl_;

   gl.glGenTextures(1, &texture);
   gl.glBindTexture(GL_TEXTURE_2D, texture);

   // TODO: Change to GL_REPEAT once a texture atlas is used
   gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   gl.glTexParameteri(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   gl.glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA,
                   view.width(),
                   view.height(),
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   pixelData.data());
   gl.glGenerateMipmap(GL_TEXTURE_2D);

   return texture;
}

} // namespace gl
} // namespace qt
} // namespace scwx
