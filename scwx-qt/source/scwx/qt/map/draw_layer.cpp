#include <scwx/qt/map/draw_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

static const std::string logPrefix_ = "scwx::qt::map::draw_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class DrawLayerImpl
{
public:
   explicit DrawLayerImpl(std::shared_ptr<MapContext> context) :
       context_ {context}, drawList_ {}, textureAtlas_ {GL_INVALID_INDEX}
   {
   }
   ~DrawLayerImpl() {}

   std::shared_ptr<MapContext>                      context_;
   std::vector<std::shared_ptr<gl::draw::DrawItem>> drawList_;
   GLuint                                           textureAtlas_;
};

DrawLayer::DrawLayer(std::shared_ptr<MapContext> context) :
    GenericLayer(context), p(std::make_unique<DrawLayerImpl>(context))
{
}
DrawLayer::~DrawLayer() = default;

void DrawLayer::Initialize()
{
   p->textureAtlas_ = p->context_->GetTextureAtlas();

   for (auto& item : p->drawList_)
   {
      item->Initialize();
   }
}

void DrawLayer::Render(const QMapbox::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = p->context_->gl();

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_2D, p->textureAtlas_);

   for (auto& item : p->drawList_)
   {
      item->Render(params);
   }
}

void DrawLayer::Deinitialize()
{
   p->textureAtlas_ = GL_INVALID_INDEX;

   for (auto& item : p->drawList_)
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
