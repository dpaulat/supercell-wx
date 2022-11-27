#include <scwx/qt/manager/imgui_manager.hpp>
#include <scwx/util/logger.hpp>

#include <imgui.h>

namespace scwx
{
namespace qt
{
namespace manager
{

static const std::string logPrefix_ = "scwx::qt::manager::imgui_manager";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ImGuiManagerImpl
{
public:
   explicit ImGuiManagerImpl() {}

   ~ImGuiManagerImpl() = default;

   std::vector<ImGuiContextInfo> contexts_ {};
};

ImGuiManager::ImGuiManager() :
    QObject(nullptr), p {std::make_unique<ImGuiManagerImpl>()}
{
}

ImGuiManager::~ImGuiManager() {}

ImGuiContext* ImGuiManager::CreateContext(const std::string& name)
{
   ImGuiContext* context = ImGui::CreateContext();
   ImGui::SetCurrentContext(context);

   // ImGui Configuration
   auto& io = ImGui::GetIO();

   // Disable automatic configuration loading/saving
   io.IniFilename = nullptr;

   // Style
   auto& style         = ImGui::GetStyle();
   style.WindowMinSize = {10.0f, 10.0f};

   // Register context
   static size_t nextId_ {0};
   p->contexts_.emplace_back(ImGuiContextInfo {nextId_++, name, context});

   // Inform observers contexts have been updated
   emit ContextsUpdated();

   return context;
}

void ImGuiManager::DestroyContext(const std::string& name)
{
   // Find context from registry
   auto it = std::find_if(p->contexts_.begin(),
                          p->contexts_.end(),
                          [&](auto& info) { return info.name_ == name; });

   if (it != p->contexts_.end())
   {
      // Destroy context and erase from index
      ImGui::SetCurrentContext(it->context_);
      ImGui::DestroyContext();
      p->contexts_.erase(it);

      // Inform observers contexts have been updated
      emit ContextsUpdated();
   }
}

std::vector<ImGuiContextInfo> ImGuiManager::contexts() const
{
   return p->contexts_;
}

ImGuiManager& ImGuiManager::Instance()
{
   static ImGuiManager instance_;
   return instance_;
}

bool ImGuiContextInfo::operator==(const ImGuiContextInfo& o) const = default;

} // namespace manager
} // namespace qt
} // namespace scwx
