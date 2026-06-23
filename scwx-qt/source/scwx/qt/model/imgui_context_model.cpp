#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <imgui.h>

// Expose required functions from internal API
void ImFontAtlasUpdateNewFrame(ImFontAtlas* atlas,
                               int          frame_count,
                               bool         renderer_has_textures);

namespace scwx::qt::model
{

static const std::string logPrefix_ = "scwx::qt::model::imgui_context_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ImGuiContextModelImpl
{
public:
   explicit ImGuiContextModelImpl() : sharedFontAtlas_ {IM_NEW(ImFontAtlas)()}
   {
   }

   ~ImGuiContextModelImpl()
   {
      // ImGui deletes the shared atlas when the last context using it is
      // destroyed; only delete here if contexts were not torn down cleanly.
      if (sharedFontAtlas_ != nullptr)
      {
         IM_DELETE(sharedFontAtlas_);
         sharedFontAtlas_ = nullptr;
      }
   }

   std::vector<ImGuiContextInfo> contexts_ {};
   ImFontAtlas*                  sharedFontAtlas_ {nullptr};

   int frameCount_ {0};
};

ImGuiContextModel::ImGuiContextModel() :
    QAbstractListModel(nullptr), p {std::make_unique<ImGuiContextModelImpl>()}
{
}

ImGuiContextModel::~ImGuiContextModel() {}

int ImGuiContextModel::rowCount(const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : static_cast<int>(p->contexts_.size());
}

QVariant ImGuiContextModel::data(const QModelIndex& index, int role) const
{
   if (!index.isValid())
   {
      return {};
   }

   const int row = index.row();
   if (row >= static_cast<int>(p->contexts_.size()) || row < 0)
   {
      return {};
   }

   switch (role)
   {
   case Qt::ItemDataRole::DisplayRole:
      return QString("%1: %2")
         .arg(p->contexts_[row].id_)
         .arg(p->contexts_[row].name_.c_str());

   case qt::types::ItemDataRole::RawDataRole:
      QVariant variant {};
      variant.setValue(p->contexts_[row]);
      return variant;
   }

   return {};
}

QModelIndex ImGuiContextModel::IndexOf(const std::string& contextName) const
{
   // Find context from registry
   auto it =
      std::find_if(p->contexts_.begin(),
                   p->contexts_.end(),
                   [&](auto& info) { return info.name_ == contextName; });

   if (it != p->contexts_.end())
   {
      const int row = it - p->contexts_.begin();
      return createIndex(row, 0, nullptr);
   }

   return {};
}

ImGuiContext* ImGuiContextModel::CreateContext(const std::string& name,
                                               bool shareFontAtlas)
{
   static size_t nextId_ {0};

   ImFontAtlas*  atlas   = shareFontAtlas ? p->sharedFontAtlas_ : nullptr;
   ImGuiContext* context = ImGui::CreateContext(atlas);
   ImGui::SetCurrentContext(context);

   // ImGui Configuration
   auto& io = ImGui::GetIO();

   // Disable automatic configuration loading/saving
   io.IniFilename = nullptr;

   // Style
   auto& style                     = ImGui::GetStyle();
   style.WindowMinSize             = {10.0f, 10.0f};
   style.WindowPadding             = {6.0f, 4.0f};
   style.Colors[ImGuiCol_Text]     = {1.00f, 1.00f, 1.00f, 0.80f};
   style.Colors[ImGuiCol_WindowBg] = {0.06f, 0.06f, 0.06f, 0.75f};

   // Register context
   const int nextPosition = static_cast<int>(p->contexts_.size());
   beginInsertRows(QModelIndex(), nextPosition, nextPosition);
   p->contexts_.emplace_back(ImGuiContextInfo {nextId_++, name, context});
   endInsertRows();

   return context;
}

void ImGuiContextModel::DestroyContext(const std::string& name)
{
   // Find context from registry
   auto it = std::find_if(p->contexts_.begin(),
                          p->contexts_.end(),
                          [&](auto& info) { return info.name_ == name; });

   if (it != p->contexts_.end())
   {
      const int     position = it - p->contexts_.begin();
      ImGuiContext* context  = it->context_;

      ImGui::SetCurrentContext(context);
      ImFontAtlas* contextAtlas = ImGui::GetIO().Fonts;
      const bool   usesSharedAtlas =
         (contextAtlas != nullptr && contextAtlas == p->sharedFontAtlas_);

      // Erase context from index
      beginRemoveRows(QModelIndex(), position, position);
      p->contexts_.erase(it);
      endRemoveRows();

      ImGui::DestroyContext();

      if (usesSharedAtlas && p->sharedFontAtlas_ != nullptr)
      {
         const bool sharedAtlasStillInUse =
            std::any_of(p->contexts_.begin(),
                        p->contexts_.end(),
                        [this](const ImGuiContextInfo& info)
                        {
                           ImGui::SetCurrentContext(info.context_);
                           return ImGui::GetIO().Fonts == p->sharedFontAtlas_;
                        });
         if (!sharedAtlasStillInUse)
         {
            p->sharedFontAtlas_ = nullptr;
         }
      }
   }
}

void ImGuiContextModel::NewFrame()
{
   static constexpr bool kRendererHasTextures_ = true;

   ImFontAtlas* atlas = p->sharedFontAtlas_;
   if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().Fonts != nullptr)
   {
      atlas = ImGui::GetIO().Fonts;
   }

   ImFontAtlasUpdateNewFrame(atlas, ++p->frameCount_, kRendererHasTextures_);
}

std::vector<ImGuiContextInfo> ImGuiContextModel::contexts() const
{
   return p->contexts_;
}

ImFontAtlas* ImGuiContextModel::font_atlas()
{
   return p->sharedFontAtlas_;
}

ImGuiContextModel& ImGuiContextModel::Instance()
{
   static ImGuiContextModel instance_ {};
   return instance_;
}

bool ImGuiContextInfo::operator==(const ImGuiContextInfo& o) const = default;

} // namespace scwx::qt::model
