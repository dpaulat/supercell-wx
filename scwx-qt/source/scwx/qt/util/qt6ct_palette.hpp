#pragma once

#include <QPalette>

namespace scwx
{
namespace qt
{
namespace util
{

/**
 * Load a color palette from a file in the same format as qt6ct uses.
 *
 * @param [in] filePath The path to the file to load from
 * @param [in] fallback A palette that will be returned if the file can not be
 * loaded
 *
 * @return The loaded palette, or the fallback palette if it could not be loaded
 */
QPalette loadColorScheme(const QString& filePath, const QPalette& fallback);

} // namespace util
} // namespace qt
} // namespace scwx
