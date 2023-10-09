#pragma once

#include <scwx/qt/types/font_types.hpp>

#include <vector>

#include <boost/gil/typedefs.hpp>

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

std::shared_ptr<boost::gil::rgba8_image_t>
LoadImageResource(const std::string& urlString);
std::vector<std::shared_ptr<boost::gil::rgba8_image_t>>
LoadImageResources(const std::vector<std::string>& urlStrings);

} // namespace ResourceManager
} // namespace manager
} // namespace qt
} // namespace scwx
