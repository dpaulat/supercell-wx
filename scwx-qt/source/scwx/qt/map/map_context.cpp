#include <scwx/qt/map/map_context.hpp>
#include <scwx/util/hash.hpp>

namespace scwx
{
namespace qt
{
namespace map
{

class MapContext::Impl
{
public:
   explicit Impl(std::shared_ptr<view::RadarProductView> radarProductView) :
       gl_ {},
       settings_ {},
       radarProductView_ {radarProductView},
       radarProductGroup_ {common::RadarProductGroup::Unknown},
       radarProduct_ {"???"},
       radarProductCode_ {0},
       shaderProgramMap_ {},
       shaderProgramMutex_ {}
   {
   }

   ~Impl() {}

   gl::OpenGLFunctions                     gl_;
   MapSettings                             settings_;
   std::shared_ptr<view::RadarProductView> radarProductView_;
   common::RadarProductGroup               radarProductGroup_;
   std::string                             radarProduct_;
   int16_t                                 radarProductCode_;

   std::unordered_map<std::pair<std::string, std::string>,
                      std::shared_ptr<gl::ShaderProgram>,
                      util::hash<std::pair<std::string, std::string>>>
              shaderProgramMap_;
   std::mutex shaderProgramMutex_;
};

MapContext::MapContext(
   std::shared_ptr<view::RadarProductView> radarProductView) :
    p(std::make_unique<Impl>(radarProductView))
{
}
MapContext::~MapContext() = default;

MapContext::MapContext(MapContext&&) noexcept            = default;
MapContext& MapContext::operator=(MapContext&&) noexcept = default;

gl::OpenGLFunctions& MapContext::gl()
{
   return p->gl_;
}

MapSettings& MapContext::settings()
{
   return p->settings_;
}

std::shared_ptr<view::RadarProductView> MapContext::radar_product_view() const
{
   return p->radarProductView_;
}

common::RadarProductGroup MapContext::radar_product_group() const
{
   return p->radarProductGroup_;
}

std::string MapContext::radar_product() const
{
   return p->radarProduct_;
}

int16_t MapContext::radar_product_code() const
{
   return p->radarProductCode_;
}

void MapContext::set_radar_product_view(
   std::shared_ptr<view::RadarProductView> radarProductView)
{
   p->radarProductView_ = radarProductView;
}

void MapContext::set_radar_product_group(
   common::RadarProductGroup radarProductGroup)
{
   p->radarProductGroup_ = radarProductGroup;
}

void MapContext::set_radar_product(const std::string& radarProduct)
{
   p->radarProduct_ = radarProduct;
}

void MapContext::set_radar_product_code(int16_t radarProductCode)
{
   p->radarProductCode_ = radarProductCode;
}

std::shared_ptr<gl::ShaderProgram>
MapContext::GetShaderProgram(const std::string& vertexPath,
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

} // namespace map
} // namespace qt
} // namespace scwx
