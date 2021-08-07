#include <scwx/qt/util/font_buffer.hpp>

#include <mutex>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace qt
{
namespace util
{

static const std::string logPrefix_ = "[scwx::qt::util::font_buffer] ";

class FontBufferImpl
{
public:
   explicit FontBufferImpl() :
       vaoId_ {GL_INVALID_INDEX},
       verticesId_ {GL_INVALID_INDEX},
       indicesId_ {GL_INVALID_INDEX},
       gpuISize_ {0},
       gpuVSize_ {0},
       dirty_ {true}
   {
   }

   ~FontBufferImpl() {}

   void RenderSetup(OpenGLFunctions& gl)
   {
      // Generate and setup VAO
      gl.glGenVertexArrays(1, &vaoId_);
      gl.glBindVertexArray(vaoId_);

      gl.glBindBuffer(GL_ARRAY_BUFFER, verticesId_);

      // vec3 aVertex
      gl.glEnableVertexAttribArray(0);
      gl.glVertexAttribPointer(
         0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);

      // vec2 aTexCoords
      gl.glEnableVertexAttribArray(1);
      gl.glVertexAttribPointer(
         1,
         2,
         GL_FLOAT,
         GL_FALSE,
         9 * sizeof(float),
         reinterpret_cast<const GLvoid*>(3 * sizeof(float)));

      // vec4 aColor
      gl.glEnableVertexAttribArray(2);
      gl.glVertexAttribPointer(
         2,
         4,
         GL_FLOAT,
         GL_FALSE,
         9 * sizeof(float),
         reinterpret_cast<const GLvoid*>(5 * sizeof(float)));

      gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesId_);
   }

   void Upload(OpenGLFunctions& gl)
   {
      if (verticesId_ == GL_INVALID_INDEX)
      {
         gl.glGenBuffers(1, &verticesId_);
      }
      if (indicesId_ == GL_INVALID_INDEX)
      {
         gl.glGenBuffers(1, &indicesId_);
      }

      GLsizei vSize = static_cast<GLsizei>(vertices_.size() * sizeof(GLfloat));
      GLsizei iSize = static_cast<GLsizei>(indices_.size() * sizeof(GLuint));

      // Always upload vertices first to avoid rendering non-existent data

      // Upload vertices
      gl.glBindBuffer(GL_ARRAY_BUFFER, verticesId_);
      if (vSize != gpuVSize_)
      {
         gl.glBufferData(
            GL_ARRAY_BUFFER, vSize, vertices_.data(), GL_DYNAMIC_DRAW);
         gpuVSize_ = vSize;
      }
      else
      {
         gl.glBufferSubData(GL_ARRAY_BUFFER, 0, vSize, vertices_.data());
      }

      // Upload indices
      gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesId_);
      if (iSize != gpuISize_)
      {
         gl.glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, iSize, indices_.data(), GL_DYNAMIC_DRAW);
      }
      else
      {
         gl.glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, iSize, indices_.data());
      }

      dirty_ = false;
   }

   GLuint  vaoId_;
   GLuint  verticesId_;
   GLuint  indicesId_;
   GLsizei gpuISize_;
   GLsizei gpuVSize_;
   bool    dirty_;

   std::vector<GLfloat> vertices_;
   std::vector<GLuint>  indices_;

   std::mutex mutex_;
};

FontBuffer::FontBuffer() : p(std::make_unique<FontBufferImpl>()) {}
FontBuffer::~FontBuffer() = default;

void FontBuffer::Clear()
{
   if (!p->indices_.empty() || !p->vertices_.empty())
   {
      std::scoped_lock lock(p->mutex_);
      p->indices_.clear();
      p->vertices_.clear();
      p->dirty_ = true;
   }
}

void FontBuffer::Push(std::initializer_list<GLuint>  indices,
                      std::initializer_list<GLfloat> vertices)
{
   if (indices.size() % 3 != 0 || vertices.size() % 9 != 0)
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Invalid push arguments, ignoring";
      return;
   }

   std::scoped_lock lock(p->mutex_);

   GLuint indexStart = static_cast<GLuint>(p->vertices_.size() / 9);

   for (GLuint index : indices)
   {
      p->indices_.push_back(index + indexStart);
   }

   p->vertices_.insert(p->vertices_.end(), vertices);
}

void FontBuffer::Render(OpenGLFunctions& gl)
{
   std::scoped_lock lock(p->mutex_);

   if (p->dirty_)
   {
      p->Upload(gl);
   }

   if (p->vaoId_ == GL_INVALID_INDEX)
   {
      p->RenderSetup(gl);
   }

   // Bind VAO for drawing
   gl.glBindVertexArray(p->vaoId_);

   gl.glDrawElements(GL_TRIANGLES,
                     static_cast<GLsizei>(p->indices_.size()),
                     GL_UNSIGNED_INT,
                     0);
}

} // namespace util
} // namespace qt
} // namespace scwx
