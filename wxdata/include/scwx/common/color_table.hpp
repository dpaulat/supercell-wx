#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/gil.hpp>

namespace scwx
{
namespace common
{

class ColorTableImpl;

/**
 * @brief Color Table
 *
 * Implementation based on:
 * Color Table File Specification
 * Mike Gibson
 * Gibson Ridge Software, LLC.  Used with permission.
 * http://www.grlevelx.com/manuals/color_tables/files_color_table.htm
 */
class ColorTable
{
public:
   explicit ColorTable();
   ~ColorTable();

   ColorTable(const ColorTable&) = delete;
   ColorTable& operator=(const ColorTable&) = delete;

   ColorTable(ColorTable&&) noexcept;
   ColorTable& operator=(ColorTable&&) noexcept;

   boost::gil::rgba8_pixel_t Color(float value) const;

   static std::shared_ptr<ColorTable> Load(const std::string& filename);

private:
   std::unique_ptr<ColorTableImpl> p;

   void ProcessLine(const std::vector<std::string>& tokenList);
};

} // namespace common
} // namespace scwx
