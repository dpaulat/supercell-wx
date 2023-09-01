#pragma once

#include <scwx/common/geographic.hpp>

#include <chrono>
#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <boost/gil/typedefs.hpp>
#include <units/angle.h>
#include <units/length.h>

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

   struct IconFile
   {
      std::size_t fileNumber_ {};
      std::size_t iconWidth_ {};
      std::size_t iconHeight_ {};
      std::size_t hotX_ {};
      std::size_t hotY_ {};
      std::string filename_ {};
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
      ItemType                              itemType_ {ItemType::Unknown};
      units::length::nautical_miles<double> threshold_ {};
   };

   struct IconDrawItem : DrawItem
   {
      IconDrawItem() { itemType_ = ItemType::Icon; }

      double                 latitude_ {};
      double                 longitude_ {};
      double                 x_ {};
      double                 y_ {};
      units::degrees<double> angle_ {};
      std::size_t            fileNumber_ {0u};
      std::size_t            iconNumber_ {0u};
      std::string            hoverText_ {};
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

   struct LineDrawItem : DrawItem
   {
      LineDrawItem() { itemType_ = ItemType::Line; }

      boost::gil::rgba8_pixel_t color_ {};
      double                    width_ {};
      std::int32_t              flags_ {};
      std::string               hoverText_ {};

      struct Element
      {
         double latitude_ {};
         double longitude_ {};
         double x_ {};
         double y_ {};
      };

      std::vector<Element> elements_ {};
   };

   struct TrianglesDrawItem : DrawItem
   {
      TrianglesDrawItem() { itemType_ = ItemType::Triangles; }

      boost::gil::rgba8_pixel_t color_ {};

      struct Element
      {
         double latitude_ {};
         double longitude_ {};
         double x_ {};
         double y_ {};

         std::optional<boost::gil::rgba8_pixel_t> color_ {};
      };

      std::vector<Element> elements_ {};
   };

   struct ImageDrawItem : DrawItem
   {
      ImageDrawItem() { itemType_ = ItemType::Image; }

      std::string imageFile_ {};

      struct Element
      {
         double latitude_ {};
         double longitude_ {};
         double x_ {};
         double y_ {};
         double tu_ {};
         double tv_ {};
      };

      std::vector<Element> elements_ {};
   };

   struct PolygonDrawItem : DrawItem
   {
      PolygonDrawItem() { itemType_ = ItemType::Polygon; }

      boost::gil::rgba8_pixel_t color_ {};

      struct Element
      {
         double latitude_ {};
         double longitude_ {};
         double x_ {};
         double y_ {};

         std::optional<boost::gil::rgba8_pixel_t> color_ {};
      };

      std::vector<std::vector<Element>> contours_ {};
      scwx::common::Coordinate          center_ {};
   };

   bool IsValid() const;

   /**
    * @brief Gets the list of draw items defined in the placefile
    *
    * @return vector of draw item pointers
    */
   std::vector<std::shared_ptr<DrawItem>> GetDrawItems();

   std::vector<std::shared_ptr<const IconFile>> icon_files();

   std::string                                            name() const;
   std::string                                            title() const;
   std::chrono::seconds                                   refresh() const;
   std::unordered_map<std::size_t, std::shared_ptr<Font>> fonts();
   std::shared_ptr<Font>                                  font(std::size_t i);

   static std::shared_ptr<Placefile> Load(const std::string& filename);
   static std::shared_ptr<Placefile> Load(const std::string& name,
                                          std::istream&      is);

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace gr
} // namespace scwx
