#pragma once

#include <memory>
#include <string>

namespace scwx
{
namespace qt
{
namespace util
{

class ImGui
{
public:
   explicit ImGui();
   ~ImGui();

   ImGui(const ImGui&)            = delete;
   ImGui& operator=(const ImGui&) = delete;

   ImGui(ImGui&&) noexcept;
   ImGui& operator=(ImGui&&) noexcept;

   void DrawTooltip(const std::string& hoverText);

   static ImGui& Instance();

private:
   class Impl;

   std::unique_ptr<Impl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
