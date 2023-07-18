#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <boost/gil/typedefs.hpp>

namespace scwx
{
namespace gr
{

/**
 * @brief Place File
 *
 * Implementation based on:
 * Place File Specification
 * Mike Gibson
 * Gibson Ridge Software, LLC.  Used with permission.
 * http://www.grlevelx.com/manuals/gis/files_places.htm
 */
class Placefile
{
public:
   explicit Placefile();
   ~Placefile();

   Placefile(const Placefile&)            = delete;
   Placefile& operator=(const Placefile&) = delete;

   Placefile(Placefile&&) noexcept;
   Placefile& operator=(Placefile&&) noexcept;

   bool IsValid() const;

   static std::shared_ptr<Placefile> Load(const std::string& filename);
   static std::shared_ptr<Placefile> Load(std::istream& is);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace gr
} // namespace scwx
