#pragma once

#include <scwx/qt/types/font_types.hpp>
#include <scwx/qt/util/font.hpp>

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

int                         FontId(types::Font font);
std::shared_ptr<util::Font> Font(types::Font font);

bool LoadImageResource(const std::string& urlString);
void LoadImageResources(const std::vector<std::string>& urlStrings);

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
