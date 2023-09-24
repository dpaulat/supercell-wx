#pragma once

#include <scwx/qt/types/imgui_font.hpp>

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

   static std::shared_ptr<FontManager> Instance();

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace manager
} // namespace qt
} // namespace scwx
