#pragma once

#include <scwx/qt/types/imgui_font.hpp>
#include <scwx/qt/types/text_types.hpp>

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
   Q_DISABLE_COPY_MOVE(FontManager)

public:
   explicit FontManager();
   ~FontManager();

   std::shared_mutex& imgui_font_atlas_mutex();
   std::uint64_t      imgui_fonts_build_count() const;

   std::shared_ptr<types::ImGuiFont>
   GetImGuiFont(types::FontCategory fontCategory);
   std::shared_ptr<types::ImGuiFont>
   LoadImGuiFont(const std::string&               family,
                 const std::vector<std::string>&  styles,
                 units::font_size::points<double> size,
                 bool                             loadIfNotFound = true);

   void LoadApplicationFont(const std::string& filename);
   void InitializeFonts();

   static FontManager& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
