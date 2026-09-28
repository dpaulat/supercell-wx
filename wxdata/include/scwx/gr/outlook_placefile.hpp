#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <boost/gil/typedefs.hpp>

namespace scwx
{
namespace gr
{

/**
 * @brief Maps a GeoJSON feature's integer "dn" property to a color, for
 * outlook sources that don't self-color like SPC's do (WPC's Excessive
 * Rainfall Outlook provides only "dn": 1-4, no fill/stroke).
 */
struct OutlookDnColor
{
   int                       dn;
   boost::gil::rgba8_pixel_t color;
};

/**
 * @brief How an outlook's risk polygons should be drawn.
 *
 * Solid fills stack poorly when categories overlap (the common case --
 * a Slight risk area typically contains a smaller Moderate area inside
 * it, etc.): the higher category's fill just paints over the lower one,
 * and semi-transparent fills compound into a muddy blend where several
 * overlap. Contour draws each ring as an outline instead (Place File
 * `Line:`, not `Polygon:`), which reads more like how these products are
 * conventionally shown on real weather maps -- nested colored bands
 * rather than nested colored fills. Applies equally to WPC's QPF (see
 * ConvertOutlookKmzToPlacefile()) -- confirmed live that its own
 * threshold bands are real per-amount vector polygons, not a raster
 * grid, despite an earlier assumption here to the contrary; a genuinely
 * raster product would be the actual separate, unrelated concern this
 * enum doesn't apply to.
 */
enum class OutlookRenderMode
{
   Fill,
   Contour
};

/**
 * @brief Converts a GeoJSON FeatureCollection of outlook risk polygons
 * (SPC convective/fire weather outlooks, WPC Excessive Rainfall Outlook --
 * all publish the same "one Polygon/MultiPolygon feature per risk
 * category" shape) into real Place File text, so the result can be loaded
 * through the exact same path as any other placefile via
 * Placefile::Load() -- reuses the placefile pipeline wholesale rather
 * than a bespoke renderer.
 *
 * Each GeoJSON feature becomes one `Color:`/`Polygon:`/`End:` block; each
 * ring of each polygon in the feature's geometry becomes one contour.
 * GeoJSON rings are already self-closing per RFC 7946 (first position
 * repeats as the last), which is exactly the Place File spec's own
 * closing convention too, so ring points are emitted as-is.
 * Features with no usable color (an empty "fill"/"stroke" string, as SPC
 * emits on a "no areas" day) are skipped.
 *
 * @param geoJson A GeoJSON FeatureCollection. Each feature should carry
 * either a "fill"/"stroke" hex color string (e.g. "#C1E9C1", the schema
 * SPC's own outlook endpoints use), or -- if `dnColorTable` is non-empty
 * -- an integer "dn" property matched against it (the schema WPC's
 * Excessive Rainfall Outlook uses, which has no color of its own).
 * @param title Placefile Title: line.
 * @param refreshSeconds Placefile RefreshSeconds: line.
 * @param dnColorTable Fallback dn -> color lookup, used only for a
 * feature whose fill/stroke are absent or unparseable. Pass an empty
 * vector (the default) for self-colored sources like SPC.
 * @param renderMode Fill (one Polygon: block per feature, every ring a
 * contour of it) or Contour (one Line: block per ring instead -- see
 * OutlookRenderMode).
 * @param contourLineWidth Place File line width, pixels; only meaningful
 * for OutlookRenderMode::Contour.
 *
 * @return Place File text, or an empty string if geoJson could not be
 * parsed as a FeatureCollection.
 */
std::string ConvertOutlookGeoJsonToPlacefile(
   const std::string&                 geoJson,
   const std::string&                 title,
   int                                refreshSeconds,
   const std::vector<OutlookDnColor>& dnColorTable = {},
   OutlookRenderMode                  renderMode   = OutlookRenderMode::Fill,
   unsigned int                       contourLineWidth = 3);

/**
 * @brief Converts a WPC QPF-style KMZ (a zipped "doc.kml") archive into
 * real Place File text -- the KML-source counterpart to
 * ConvertOutlookGeoJsonToPlacefile() above, for outlook sources published
 * as KML rather than GeoJSON (WPC's QPF products, unlike its own
 * Excessive Rainfall Outlook or any of SPC's outlooks, publish only KML/
 * KMZ, no GeoJSON equivalent -- confirmed live, not assumed).
 *
 * Each `<Placemark>` becomes one feature: its color comes from the
 * `<Style>` its `<styleUrl>` references (a KML `aabbggrr` hex string, a
 * different byte order from GeoJSON's `#rrggbb`), and every
 * `<LinearRing>` under it -- whether from `<outerBoundaryIs>` or
 * `<innerBoundaryIs>`, real WPC QPF KML uses both -- becomes one contour,
 * the same "don't distinguish sub-polygon from hole" flattening
 * ConvertOutlookGeoJsonToPlacefile() already uses. A Placemark with no
 * resolvable style or no rings is skipped, same as a GeoJSON feature with
 * no usable color.
 *
 * @param kmzBytes Raw KMZ archive bytes (a zip archive containing
 * "doc.kml"), e.g. as downloaded directly into a std::string.
 * @param title Placefile Title: line.
 * @param refreshSeconds Placefile RefreshSeconds: line.
 * @param renderMode Fill or Contour -- see OutlookRenderMode. Real WPC QPF
 * threshold bands nest the same way SPC/WPC's other outlooks do (a 2-inch
 * band typically contains a smaller 3-inch band, etc.), so Contour is the
 * same conventionally-appropriate default as everywhere else in this
 * file, not Fill's own default above (kept there only for
 * ConvertOutlookGeoJsonToPlacefile()'s pre-existing signature).
 * @param contourLineWidth Place File line width, pixels; only meaningful
 * for OutlookRenderMode::Contour.
 *
 * @return Place File text, or an empty string if kmzBytes could not be
 * unzipped or its "doc.kml" could not be parsed as KML.
 */
std::string ConvertOutlookKmzToPlacefile(
   const std::string& kmzBytes,
   const std::string& title,
   int                refreshSeconds,
   OutlookRenderMode  renderMode       = OutlookRenderMode::Contour,
   unsigned int       contourLineWidth = 3);

/**
 * @brief WPC Excessive Rainfall Outlook's dn (1-4) -> color table.
 *
 * Sourced directly from WPC's own products, not approximated: the fill
 * colors and ~46% opacity (alpha 0x75) come from the real KML style
 * definitions at https://www.wpc.ncep.noaa.gov/kml/ero/
 * Day_1_Excessive_Rainfall_Outlook.kmz (confirmed live for dn 1
 * "Marginal" and dn 2 "Slight" -- the only categories active when
 * checked); dn 3 "Moderate" and dn 4 "High" come from WPC's own legend
 * graphic (https://www.wpc.ncep.noaa.gov/qpf/web_ero/ero_legend.svg),
 * whose color order (magenta/red/yellow/green, most to least severe)
 * matches the two KML-confirmed colors exactly, corroborating the
 * mapping. Marginal=green, Slight=yellow, Moderate=red, High=magenta is
 * also WPC's well-documented public convention for this product.
 */
extern const std::vector<OutlookDnColor> kWpcEroColorTable;

} // namespace gr
} // namespace scwx
