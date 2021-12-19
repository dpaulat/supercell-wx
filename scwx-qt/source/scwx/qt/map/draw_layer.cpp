#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>

#include <boost/log/trivial.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "[scwx::qt::map::draw_layer] ";

class DrawLayerImpl
{
public:
   explicit DrawLayerImpl(std::shared_ptr<MapContext> context) :
       shaderProgram_ {context->gl_}, uMVPMatrixLocation_(GL_INVALID_INDEX)
   {
   }

   ~DrawLayerImpl() {}

   gl::ShaderProgram shaderProgram_;
   GLint             uMVPMatrixLocation_;

   std::vector<std::shared_ptr<gl::draw::DrawItem>> drawList_;
};

DrawLayer::DrawLayer(std::shared_ptr<MapContext> context) :
    GenericLayer(context), p(std::make_unique<DrawLayerImpl>(context))
{
}
DrawLayer::~DrawLayer() = default;

void DrawLayer::Initialize()
{
   gl::OpenGLFunctions& gl = context()->gl_;

   p->shaderProgram_.Load(":/gl/color.vert", ":/gl/color.frag");

   p->uMVPMatrixLocation_ =
      gl.glGetUniformLocation(p->shaderProgram_.id(), "uMVPMatrix");
   if (p->uMVPMatrixLocation_ == -1)
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Could not find uMVPMatrix";
   }

   p->shaderProgram_.Use();

   for (auto item : p->drawList_)
   {
      item->Initialize();
   }
}

void DrawLayer::Render(const QMapbox::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = context()->gl_;

   p->shaderProgram_.Use();

   glm::mat4 projection = glm::ortho(0.0f,
                                     static_cast<float>(params.width),
                                     0.0f,
                                     static_cast<float>(params.height));

   gl.glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(projection));

   for (auto item : p->drawList_)
   {
      item->Render();
   }
}

void DrawLayer::Deinitialize()
{
   for (auto item : p->drawList_)
   {
      item->Deinitialize();
   }
}

void DrawLayer::AddDrawItem(std::shared_ptr<gl::draw::DrawItem> drawItem)
{
   p->drawList_.push_back(drawItem);
}

} // namespace map
} // namespace qt
} // namespace scwx
