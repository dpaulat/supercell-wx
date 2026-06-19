#pragma once

#include <scwx/qt/types/imgui_font.hpp>
#include <scwx/qt/types/font_types.hpp>
#include <scwx/qt/types/text_types.hpp>

#include <shared_mutex>

#include <QFont>
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

   int GetFontId(types::Font font) const;
   std::pair<std::shared_ptr<types::ImGuiFont>, units::font_size::pixels<float>>
         GetImGuiFont(types::FontCategory fontCategory);
   QFont GetQFont(types::FontCategory fontCategory);
   std::shared_ptr<types::ImGuiFont>
   LoadImGuiFont(const std::string&              family,
                 const std::vector<std::string>& styles,
                 bool                            loadIfNotFound = true);

   void LoadApplicationFont(types::Font font, const std::string& filename);
   void InitializeFonts();

   static units::font_size::pixels<float>
   ImFontSize(units::font_size::pixels<double> size);

   static FontManager& Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
