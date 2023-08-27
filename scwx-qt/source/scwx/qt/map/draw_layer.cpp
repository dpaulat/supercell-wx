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

   std::uint64_t textureAtlasBuildCount_ {};
};

DrawLayer::DrawLayer(const std::shared_ptr<MapContext>& context) :
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

void DrawLayer::Render(const QMapLibreGL::CustomLayerRenderParameters& params)
{
   gl::OpenGLFunctions& gl = p->context_->gl();
   p->textureAtlas_        = p->context_->GetTextureAtlas();

   // Determine if the texture atlas changed since last render
   std::uint64_t newTextureAtlasBuildCount =
      p->context_->texture_buffer_count();
   bool textureAtlasChanged =
      newTextureAtlasBuildCount != p->textureAtlasBuildCount_;

   gl.glActiveTexture(GL_TEXTURE0);
   gl.glBindTexture(GL_TEXTURE_2D, p->textureAtlas_);

   for (auto& item : p->drawList_)
   {
      item->Render(params, textureAtlasChanged);
   }

   p->textureAtlasBuildCount_ = newTextureAtlasBuildCount;
}

void DrawLayer::Deinitialize()
{
   p->textureAtlas_ = GL_INVALID_INDEX;

   for (auto& item : p->drawList_)
   {
      item->Deinitialize();
   }
}

bool DrawLayer::RunMousePicking(
   const QMapLibreGL::CustomLayerRenderParameters& params)
{
   bool itemPicked = false;

   // For each draw item in the draw list in reverse
   for (auto it = p->drawList_.rbegin(); it != p->drawList_.rend(); ++it)
   {
      // Run mouse picking on each draw item
      if ((*it)->RunMousePicking(params))
      {
         // If a draw item was picked, don't process additional items
         itemPicked = true;
         break;
      }
   }

   return itemPicked;
}

void DrawLayer::AddDrawItem(const std::shared_ptr<gl::draw::DrawItem>& drawItem)
{
   p->drawList_.push_back(drawItem);
}

} // namespace map
} // namespace qt
} // namespace scwx
