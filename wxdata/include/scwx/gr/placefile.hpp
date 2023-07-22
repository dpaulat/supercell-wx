#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <boost/gil/typedefs.hpp>
#include <boost/units/quantity.hpp>
#include <boost/units/systems/si/length.hpp>

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

   enum class ItemType
   {
      Icon,
      Font,
      Text,
      Line,
      Triangles,
      Image,
      Polygon,
      Unknown
   };

   struct Font
   {
      std::size_t  fontNumber_ {};
      std::size_t  pixels_ {};
      std::int32_t flags_ {};
      std::string  face_ {};
   };

   struct DrawItem
   {
      ItemType itemType_ {ItemType::Unknown};
      boost::units::quantity<boost::units::si::length> threshold_ {};
   };

   struct TextDrawItem : DrawItem
   {
      TextDrawItem() { itemType_ = ItemType::Text; }

      boost::gil::rgba8_pixel_t color_ {};
      double                    latitude_ {};
      double                    longitude_ {};
      double                    x_ {};
      double                    y_ {};
      std::size_t               fontNumber_ {0u};
      std::string               text_ {};
      std::string               hoverText_ {};
   };

   bool IsValid() const;

   /**
    * @brief Gets the list of draw items defined in the placefile
    *
    * @return vector of draw item pointers
    */
   std::vector<std::shared_ptr<DrawItem>> GetDrawItems();

   std::unordered_map<std::size_t, std::shared_ptr<Font>> fonts();
   std::shared_ptr<Font>                                  font(std::size_t i);

   static std::shared_ptr<Placefile> Load(const std::string& filename);
   static std::shared_ptr<Placefile> Load(std::istream& is);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace gr
} // namespace scwx
