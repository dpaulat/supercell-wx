#pragma once

#include <scwx/qt/types/imgui_font.hpp>

#include <shared_mutex>

#include <QObject>

namespace scwx
{
namespace qt
{
namespace manager
{

class FontManager : public QObject
{
   Q_OBJECT

public:
   explicit FontManager();
   ~FontManager();

   std::shared_mutex& imgui_font_atlas_mutex();

   std::shared_ptr<types::ImGuiFont>
   GetImGuiFont(const std::string&               family,
                const std::vector<std::string>&  styles,
                units::font_size::points<double> size);

   void LoadApplicationFont(const std::string& filename);

   static FontManager& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
