#pragma once

#include <memory>
#include <string>

#include <QObject>

struct ImGuiContext;

namespace scwx
{
namespace qt
{
namespace manager
{

class ImGuiManagerImpl;

struct ImGuiContextInfo
{
   size_t        id_ {};
   std::string   name_ {};
   ImGuiContext* context_ {};

   bool operator==(const ImGuiContextInfo& o) const;
};

class ImGuiManager : public QObject
{
private:
   Q_OBJECT
   Q_DISABLE_COPY(ImGuiManager)

public:
   explicit ImGuiManager();
   ~ImGuiManager();

   ImGuiContext* CreateContext(const std::string& name);
   void          DestroyContext(const std::string& name);

   std::vector<ImGuiContextInfo> contexts() const;

   static ImGuiManager& Instance();

signals:
   void ContextsUpdated();

private:
   friend class ImGuiManagerImpl;
   std::unique_ptr<ImGuiManagerImpl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
