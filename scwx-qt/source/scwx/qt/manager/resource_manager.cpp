#include <scwx/qt/manager/resource_manager.hpp>
#include <scwx/qt/config/county_database.hpp>
#include <scwx/qt/util/font.hpp>
#include <scwx/qt/util/texture_atlas.hpp>
#include <scwx/util/logger.hpp>

#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_qt.hpp>
#include <imgui.h>
#include <QOffscreenSurface>
#include <QOpenGLContext>

namespace scwx
{
namespace qt
{
namespace manager
{
namespace ResourceManager
{
static const std::string logPrefix_ = "scwx::qt::manager::ResourceManager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static void InitializeImGui();
static void LoadFonts();
static void LoadTextures();
static void ShutdownImGui();

static std::unique_ptr<QOffscreenSurface> surface_ {};
static ImGuiContext*                      imGuiContext_ {};

void Initialize()
{
   config::CountyDatabase::Initialize();

   InitializeImGui();
   LoadFonts();
   LoadTextures();
}

void Shutdown()
{
   ShutdownImGui();
}

static void InitializeImGui()
{
   // Create OpenGL Offscreen Surface
   surface_ = std::make_unique<QOffscreenSurface>();
   surface_->create();
   if (!QOpenGLContext::globalShareContext()->makeCurrent(surface_.get()))
   {
      logger_->warn("Failed to initialize offscreen surface");
   }

   // Initialize ImGui
   imGuiContext_ = ImGui::CreateContext();
   ImGui_ImplQt_Init();
   ImGui_ImplOpenGL3_Init();
   QOpenGLContext::globalShareContext()->doneCurrent();

   // ImGui Configuration
   auto& io = ImGui::GetIO();

   // Disable automatic configuration loading/saving
   io.IniFilename = nullptr;

   // Style
   auto& style = ImGui::GetStyle();

   style.WindowMinSize = {10.0f, 10.0f};
}

static void LoadFonts()
{
   util::Font::Create(":/res/fonts/din1451alt.ttf");
   util::Font::Create(":/res/fonts/din1451alt_g.ttf");
}

static void LoadTextures()
{
   util::TextureAtlas& textureAtlas = util::TextureAtlas::Instance();
   textureAtlas.RegisterTexture("lines/default-1x7",
                                ":/res/textures/lines/default-1x7.png");
   textureAtlas.RegisterTexture("lines/test-pattern",
                                ":/res/textures/lines/test-pattern.png");
   textureAtlas.BuildAtlas(8, 8);
}

static void ShutdownImGui()
{
   QOpenGLContext::globalShareContext()->makeCurrent(surface_.get());
   ImGui::SetCurrentContext(imGuiContext_);
   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplQt_Shutdown();
   ImGui::DestroyContext();
   QOpenGLContext::globalShareContext()->doneCurrent();
}

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
