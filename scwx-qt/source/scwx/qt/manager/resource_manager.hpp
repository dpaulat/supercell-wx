#pragma once

#include <scwx/qt/types/font_types.hpp>

namespace scwx
{
namespace qt
{
namespace manager
{
namespace ResourceManager
{

void Initialize();
void Shutdown();

int FontId(types::Font font);

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
