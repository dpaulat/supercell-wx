#include <scwx/qt/model/imgui_context_model.hpp>
#include <scwx/qt/types/qt_types.hpp>
#include <scwx/util/logger.hpp>

#include <imgui.h>

namespace scwx
{
namespace qt
{
namespace model
{

static const std::string logPrefix_ = "scwx::qt::model::imgui_context_model";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

class ImGuiContextModelImpl
{
public:
   explicit ImGuiContextModelImpl() {}

   ~ImGuiContextModelImpl() = default;

   std::vector<ImGuiContextInfo> contexts_ {};
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
   if (row >= p->contexts_.size() || row < 0)
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

ImGuiContext* ImGuiContextModel::CreateContext(const std::string& name)
{
   static size_t nextId_ {0};

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
      const int position = it - p->contexts_.begin();

      // Destroy context
      ImGui::SetCurrentContext(it->context_);
      ImGui::DestroyContext();

      // Erase context from index
      beginRemoveRows(QModelIndex(), position, position);
      p->contexts_.erase(it);
      endRemoveRows();
   }
}

std::vector<ImGuiContextInfo> ImGuiContextModel::contexts() const
{
   return p->contexts_;
}

ImGuiContextModel& ImGuiContextModel::Instance()
{
   static ImGuiContextModel instance_ {};
   return instance_;
}

bool ImGuiContextInfo::operator==(const ImGuiContextInfo& o) const = default;

} // namespace model
} // namespace qt
} // namespace scwx
