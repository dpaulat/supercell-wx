#include <scwx/qt/draw/map_annotations_draw_item.hpp>
#include <scwx/qt/map/map_annotation_model.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

#include <scwx/qt/render/rhi_colored_geometry.hpp>
#include <scwx/qt/render/rhi_vulkan_overlay.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <numbers>
#include <type_traits>
#include <unordered_set>
#include <unordered_map>

#include <GeographicLib/Geodesic.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Geometry/render math here intentionally uses tuned constants that clang-tidy
// interop patterns that clang-tidy flags noisily without improving clarity.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,modernize-use-auto,cppcoreguidelines-pro-type-cstyle-cast,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-pro-bounds-pointer-arithmetic,performance-no-int-to-ptr)
namespace scwx::qt::draw
{

static const std::string logPrefix_ =
   "scwx::qt::draw::map_annotations_draw_item";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFloatsPerVertex = 11;

static constexpr double kLatitudeMax = 85.051128779806604;
static constexpr double kPiOver4     = 0.785398163397448309615660825;
static constexpr double kPiOver360   = 0.00872664625997164788461845361111;
static constexpr double kRad2Deg     = 57.295779513082320876798156332941;

static glm::vec2 AnnotationDeltaScreenCoordinate(float            latitude,
                                                 float            longitude,
                                                 const glm::vec2& origin)
{
   const double clampedLat =
      std::clamp(static_cast<double>(latitude), -kLatitudeMax, kLatitudeMax);
   const double deltaLat = clampedLat - static_cast<double>(origin.x);
   const double deltaLon =
      static_cast<double>(longitude) - static_cast<double>(origin.y);
   const double y =
      kRad2Deg * std::log(std::tan(kPiOver4 +
                                   (static_cast<double>(origin.x) + deltaLat) *
                                      kPiOver360)) -
      kRad2Deg * std::log(std::tan(kPiOver4 +
                                   static_cast<double>(origin.x) * kPiOver360));

   return {static_cast<float>(deltaLon), static_cast<float>(y)};
}

static void AppendAnnotationTriangleVertices(std::vector<float>&       out,
                                             const std::vector<float>& in,
                                             std::size_t               base,
                                             const glm::mat4& mapMatrix,
                                             const glm::vec2& origin)
{
   for (std::size_t i = 0; i < 3; ++i)
   {
      const std::size_t vertexBase = base + i * kFloatsPerVertex;
      const glm::vec2   p          = AnnotationDeltaScreenCoordinate(
         in[vertexBase + 0], in[vertexBase + 1], origin);
      const glm::vec4 clip = mapMatrix * glm::vec4 {p, 0.0f, 1.0f};

      out.push_back(clip.x);
      out.push_back(clip.y);
      out.push_back(0.0f);
      out.push_back(in[vertexBase + 2]);
      out.push_back(in[vertexBase + 3]);
      out.push_back(in[vertexBase + 4]);
      out.push_back(in[vertexBase + 5]);
   }
}

static std::vector<float>
BuildColoredAnnotationVertices(const std::vector<float>& in,
                               std::uint32_t             vertexCount,
                               const glm::mat4&          mapMatrix,
                               const glm::vec2&          origin)
{
   std::vector<float> out;
   if (vertexCount <= 0)
   {
      return out;
   }

   const std::size_t count = static_cast<std::size_t>(vertexCount);
   out.reserve(count * 7);
   for (std::size_t vertex = 0; vertex + 2 < count; vertex += 3)
   {
      const std::size_t base       = vertex * kFloatsPerVertex;
      const float       arcLen     = in[base + 6];
      const float       dashPeriod = in[base + 9];
      const float       dashDuty   = in[base + 10];
      if (dashPeriod > 1.0e-3f &&
          std::fmod(arcLen, dashPeriod) > dashPeriod * dashDuty)
      {
         continue;
      }

      AppendAnnotationTriangleVertices(out, in, base, mapMatrix, origin);
   }

   return out;
}

static std::pair<float, float> DashAttribs(const map::MapAnnotationStyle& st)
{
   if (st.strokeStyle == map::MapAnnotationStrokeStyle::Dashed)
   {
      return {static_cast<float>(st.dashPeriodM.value()), 0.55f};
   }
   return {0.0f, 1.0f};
}

static double NormalizeAzimuthDegrees(double deg)
{
   deg = std::fmod(deg + 540.0, 360.0);
   return deg - 180.0;
}

static void AppendVertex(std::vector<float>&         out,
                         const common::Coordinate&   c,
                         const std::array<float, 4>& rgba,
                         float                       arcLen,
                         float                       hatchX,
                         float                       hatchY,
                         float                       dashPeriodM,
                         float                       dashDuty)
{
   out.push_back(static_cast<float>(c.latitude_));
   out.push_back(static_cast<float>(c.longitude_));
   out.push_back(rgba[0]);
   out.push_back(rgba[1]);
   out.push_back(rgba[2]);
   out.push_back(rgba[3]);
   out.push_back(arcLen);
   out.push_back(hatchX);
   out.push_back(hatchY);
   out.push_back(dashPeriodM);
   out.push_back(dashDuty);
}

static void AppendTriangle(std::vector<float>&         out,
                           const common::Coordinate&   c0,
                           const common::Coordinate&   c1,
                           const common::Coordinate&   c2,
                           const std::array<float, 4>& rgba,
                           float                       arcLen,
                           float                       dashPeriodM,
                           float                       dashDuty)
{
   const float hx = static_cast<float>(
      c0.longitude_ * 111320.0 * std::cos(c0.latitude_ * 0.017453292519943295));
   const float hy = static_cast<float>(c0.latitude_ * 110540.0);
   AppendVertex(out, c0, rgba, arcLen, hx, hy, dashPeriodM, dashDuty);
   const float hx1 = static_cast<float>(
      c1.longitude_ * 111320.0 * std::cos(c1.latitude_ * 0.017453292519943295));
   const float hy1 = static_cast<float>(c1.latitude_ * 110540.0);
   AppendVertex(out, c1, rgba, arcLen, hx1, hy1, dashPeriodM, dashDuty);
   const float hx2 = static_cast<float>(
      c2.longitude_ * 111320.0 * std::cos(c2.latitude_ * 0.017453292519943295));
   const float hy2 = static_cast<float>(c2.latitude_ * 110540.0);
   AppendVertex(out, c2, rgba, arcLen, hx2, hy2, dashPeriodM, dashDuty);
}

static void AppendSegmentQuad(std::vector<float>&         out,
                              const common::Coordinate&   la,
                              const common::Coordinate&   ra,
                              const common::Coordinate&   lb,
                              const common::Coordinate&   rb,
                              const std::array<float, 4>& rgba,
                              float                       arcLa,
                              float                       arcLb,
                              float                       dashPeriodM,
                              float                       dashDuty)
{
   (void) arcLb;
   AppendTriangle(out, la, ra, rb, rgba, arcLa, dashPeriodM, dashDuty);
   AppendTriangle(out, la, rb, lb, rgba, arcLa, dashPeriodM, dashDuty);
}

static void GeoStrokeSegmentCorners(const common::Coordinate&        a,
                                    const common::Coordinate&        b,
                                    units::length::meters<double>    halfWidthM,
                                    const ::GeographicLib::Geodesic& geod,
                                    double&                          s12Out,
                                    common::Coordinate&              leftA,
                                    common::Coordinate&              rightA,
                                    common::Coordinate&              leftB,
                                    common::Coordinate&              rightB)
{
   double azi1 {};
   double azi2 {};
   s12Out = geod.Inverse(
      a.latitude_, a.longitude_, b.latitude_, b.longitude_, azi1, azi2);

   // azi1 / azi2 are both forward along the geodesic (at A toward B, at B as
   // the segment arrives). Use the same ±90° convention at both ends so the
   // quad does not swap ribbon sides (twisted strip → one-sided sawteeth).
   leftA = util::GeographicLib::GetCoordinate(
      a,
      units::angle::degrees<double> {NormalizeAzimuthDegrees(azi1 - 90.0)},
      halfWidthM);
   rightA = util::GeographicLib::GetCoordinate(
      a,
      units::angle::degrees<double> {NormalizeAzimuthDegrees(azi1 + 90.0)},
      halfWidthM);

   leftB = util::GeographicLib::GetCoordinate(
      b,
      units::angle::degrees<double> {NormalizeAzimuthDegrees(azi2 - 90.0)},
      halfWidthM);
   rightB = util::GeographicLib::GetCoordinate(
      b,
      units::angle::degrees<double> {NormalizeAzimuthDegrees(azi2 + 90.0)},
      halfWidthM);
}

static glm::vec2 ScreenLatLon(const common::Coordinate& c)
{
   return util::maplibre::LatLongToScreenCoordinate(
      {c.latitude_, c.longitude_});
}

/** Fills both wedges at a polyline joint (geodesic offset corners rarely align
 * in screen space). A single-sided bevel leaves visible gaps on curves; two
 * triangles overlap slightly on the inside of the turn but keep the stroke
 * solid. */
static void AppendJoinBevelBothSides(std::vector<float>&         out,
                                     const common::Coordinate&   prevEndLeft,
                                     const common::Coordinate&   prevEndRight,
                                     const common::Coordinate&   currStartLeft,
                                     const common::Coordinate&   currStartRight,
                                     const common::Coordinate&   pivot,
                                     const std::array<float, 4>& rgba,
                                     float                       arcLen,
                                     float                       dashPeriodM,
                                     float                       dashDuty,
                                     const glm::vec2&            p0,
                                     const glm::vec2&            p1,
                                     const glm::vec2&            p2)
{
   glm::vec2   t0 = p1 - p0;
   glm::vec2   t1 = p2 - p1;
   const float l0 = glm::length(t0);
   const float l1 = glm::length(t1);
   if (l0 < 1e-7f || l1 < 1e-7f)
   {
      return;
   }
   t0 /= l0;
   t1 /= l1;
   const float cross = t0.x * t1.y - t0.y * t1.x;
   if (std::abs(cross) < 1e-8f)
   {
      return;
   }
   AppendTriangle(out,
                  prevEndRight,
                  currStartRight,
                  pivot,
                  rgba,
                  arcLen,
                  dashPeriodM,
                  dashDuty);
   AppendTriangle(out,
                  prevEndLeft,
                  currStartLeft,
                  pivot,
                  rgba,
                  arcLen,
                  dashPeriodM,
                  dashDuty);
}

static void AppendPolylineStroke(std::vector<float>&                    out,
                                 const std::vector<common::Coordinate>& pts,
                                 bool                                   closed,
                                 units::length::meters<double>    strokeWidthM,
                                 const std::array<float, 4>&      rgba,
                                 const ::GeographicLib::Geodesic& geod,
                                 float                            dashPeriodM,
                                 float                            dashDuty)
{
   std::vector<common::Coordinate> ring = pts;
   if (closed && ring.size() >= 2)
   {
      const auto& a = ring.front();
      const auto& b = ring.back();
      if (a.latitude_ == b.latitude_ && a.longitude_ == b.longitude_)
      {
         ring.pop_back();
      }
   }
   const std::size_t n = ring.size();
   if (n < 2)
   {
      return;
   }
   const auto        halfW = strokeWidthM * 0.5;
   float             arcLen {0.0f};
   const std::size_t numSeg = closed ? n : n - 1;

   common::Coordinate prevEndLeft {};
   common::Coordinate prevEndRight {};
   bool               havePrev = false;

   if (closed && n >= 3)
   {
      double             sWrap {};
      common::Coordinate wrapLa;
      common::Coordinate wrapRa;
      common::Coordinate wrapLb;
      common::Coordinate wrapRb;
      GeoStrokeSegmentCorners(ring[n - 1],
                              ring[0],
                              halfW,
                              geod,
                              sWrap,
                              wrapLa,
                              wrapRa,
                              wrapLb,
                              wrapRb);
      (void) wrapLa;
      (void) wrapRa;
      prevEndLeft  = wrapLb;
      prevEndRight = wrapRb;
      havePrev     = true;
   }

   for (std::size_t s = 0; s < numSeg; ++s)
   {
      const common::Coordinate& a = closed ? ring[s % n] : ring[s];
      const common::Coordinate& b = closed ? ring[(s + 1) % n] : ring[s + 1];

      double             s12 {};
      common::Coordinate leftA;
      common::Coordinate rightA;
      common::Coordinate leftB;
      common::Coordinate rightB;
      GeoStrokeSegmentCorners(
         a, b, halfW, geod, s12, leftA, rightA, leftB, rightB);

      if (havePrev)
      {
         const common::Coordinate& p0 =
            closed ? ring[(s + n - 1) % n] : ring[s - 1];
         const common::Coordinate& p1 = closed ? ring[s % n] : ring[s];
         const common::Coordinate& p2 =
            closed ? ring[(s + 1) % n] : ring[s + 1];

         AppendJoinBevelBothSides(out,
                                  prevEndLeft,
                                  prevEndRight,
                                  leftA,
                                  rightA,
                                  p1,
                                  rgba,
                                  arcLen,
                                  dashPeriodM,
                                  dashDuty,
                                  ScreenLatLon(p0),
                                  ScreenLatLon(p1),
                                  ScreenLatLon(p2));
      }

      const float arcA = arcLen;
      arcLen += static_cast<float>(s12);
      AppendSegmentQuad(out,
                        leftA,
                        rightA,
                        leftB,
                        rightB,
                        rgba,
                        arcA,
                        arcLen,
                        dashPeriodM,
                        dashDuty);

      prevEndLeft  = leftB;
      prevEndRight = rightB;
      havePrev     = true;
   }
}

static constexpr int kRoundBrushDiskSegments = 48;
static constexpr int kJoinDiskSegments       = 12;
static constexpr int kCircleRingSegments     = 96;

static void AppendFilledGeoDisk(std::vector<float>&         fillOut,
                                const common::Coordinate&   center,
                                double                      radiusM,
                                const std::array<float, 4>& rgba,
                                int segments = kRoundBrushDiskSegments);

/** Circle and rectangle share this: closed geodesic ribbon + round-join disk
 * at every ring vertex. Bevel-only leaves seams (circle close + rect corners).
 * 12-seg disks stay much cheaper than the old 64-seg × 128-vert stamp. */
static void AppendClosedRingStroke(std::vector<float>&                    out,
                                   const std::vector<common::Coordinate>& ring,
                                   units::length::meters<double> strokeWidthM,
                                   const std::array<float, 4>&   rgba,
                                   const ::GeographicLib::Geodesic& geod,
                                   float                            dashPeriodM,
                                   float                            dashDuty)
{
   if (ring.size() < 3 || strokeWidthM.value() <= 0.0)
   {
      return;
   }

   std::array<float, 4> strokeRgba = rgba;
   strokeRgba[3]                   = std::max(strokeRgba[3], 0.95f);
   AppendPolylineStroke(
      out, ring, true, strokeWidthM, strokeRgba, geod, dashPeriodM, dashDuty);
   const double halfW = strokeWidthM.value() * 0.5;
   for (const auto& p : ring)
   {
      AppendFilledGeoDisk(out, p, halfW, strokeRgba, kJoinDiskSegments);
   }
}

static std::vector<common::Coordinate>
BuildCircleRing(const common::Coordinate& center, double radiusM)
{
   // Unique verts only — duplicated close + closed=true makes a zero-length
   // segment, bevel bails, ring gapes.
   std::vector<common::Coordinate> ring;
   ring.reserve(static_cast<std::size_t>(kCircleRingSegments));
   for (int i = 0; i < kCircleRingSegments; ++i)
   {
      const double az = 360.0 * static_cast<double>(i) /
                        static_cast<double>(kCircleRingSegments);
      ring.push_back(util::GeographicLib::GetCoordinate(
         center,
         units::angle::degrees<double> {az},
         units::length::meters<double> {radiusM}));
   }
   return ring;
}

static std::vector<common::Coordinate>
BuildRectangleRing(double minLat, double maxLat, double minLon, double maxLon)
{
   // Four corners only. Densifying sides made collinear geodesic joins skip
   // bevels and punched notches in the border.
   return {
      {minLat, minLon}, {minLat, maxLon}, {maxLat, maxLon}, {maxLat, minLon}};
}

static void AppendRingFill(std::vector<float>&                    out,
                           const std::vector<common::Coordinate>& ring,
                           const std::array<float, 4>&            rgba)
{
   if (ring.size() < 3)
   {
      return;
   }
   common::Coordinate centroid {0.0, 0.0};
   for (const auto& p : ring)
   {
      centroid.latitude_ += p.latitude_;
      centroid.longitude_ += p.longitude_;
   }
   const double n = static_cast<double>(ring.size());
   centroid.latitude_ /= n;
   centroid.longitude_ /= n;
   for (std::size_t i = 0; i < ring.size(); ++i)
   {
      AppendTriangle(out,
                     centroid,
                     ring[i],
                     ring[(i + 1) % ring.size()],
                     rgba,
                     0.0f,
                     0.0f,
                     1.0f);
   }
}

static void AppendCircleStroke(std::vector<float>&              out,
                               const common::Coordinate&        center,
                               double                           radiusM,
                               units::length::meters<double>    strokeWidthM,
                               const std::array<float, 4>&      rgba,
                               const ::GeographicLib::Geodesic& geod,
                               float                            dashPeriodM,
                               float                            dashDuty)
{
   AppendClosedRingStroke(out,
                          BuildCircleRing(center, radiusM),
                          strokeWidthM,
                          rgba,
                          geod,
                          dashPeriodM,
                          dashDuty);
}

/** Shape fills stay translucent so the map remains readable under them. */
static std::array<float, 4> ShapeFillRgba(const map::MapAnnotationStyle& style)
{
   // Soft wash under the stroke. Composited in color.frag against a backdrop
   // copy (preserve-pass SrcAlpha blend is unreliable).
   constexpr float kFillAlpha = 0.14f;
   return {style.strokeColor[0],
           style.strokeColor[1],
           style.strokeColor[2],
           kFillAlpha};
}

/** Fill stops at the stroke's inner edge (stroke is centered on the outline).
 */
static double InsetFillRadiusM(double                        outlineRadiusM,
                               units::length::meters<double> strokeWidthM)
{
   return std::max(0.0, outlineRadiusM - strokeWidthM.value() * 0.5);
}

static void AppendRectangleStrokeAndFill(std::vector<float>&       strokeOut,
                                         std::vector<float>&       fillOut,
                                         const common::Coordinate& c1,
                                         const common::Coordinate& c2,
                                         const map::MapAnnotationStyle&   style,
                                         const ::GeographicLib::Geodesic& geod)
{
   const double minLat = std::min(c1.latitude_, c2.latitude_);
   const double maxLat = std::max(c1.latitude_, c2.latitude_);
   const double minLon = std::min(c1.longitude_, c2.longitude_);
   const double maxLon = std::max(c1.longitude_, c2.longitude_);

   if (maxLat <= minLat || maxLon <= minLon)
   {
      return;
   }

   const std::vector<common::Coordinate> ring =
      BuildRectangleRing(minLat, maxLat, minLon, maxLon);

   if (style.polygonFill)
   {
      AppendRingFill(fillOut, ring, ShapeFillRgba(style));
   }

   const auto dash = DashAttribs(style);
   AppendClosedRingStroke(strokeOut,
                          ring,
                          style.strokeWidthM,
                          style.strokeColor,
                          geod,
                          dash.first,
                          dash.second);
}

static void AppendFilledGeoDisk(std::vector<float>&         fillOut,
                                const common::Coordinate&   center,
                                double                      radiusM,
                                const std::array<float, 4>& rgba,
                                int                         segments)
{
   if (radiusM <= 0.0 || segments < 3)
   {
      return;
   }
   std::vector<common::Coordinate> ring;
   ring.reserve(static_cast<std::size_t>(segments) + 1);
   for (int i = 0; i <= segments; ++i)
   {
      const double az =
         360.0 * static_cast<double>(i) / static_cast<double>(segments);
      ring.push_back(util::GeographicLib::GetCoordinate(
         center,
         units::angle::degrees<double> {az},
         units::length::meters<double> {radiusM}));
   }
   for (int i = 0; i < segments; ++i)
   {
      AppendTriangle(fillOut,
                     center,
                     ring[static_cast<std::size_t>(i)],
                     ring[static_cast<std::size_t>(i) + 1],
                     rgba,
                     0.0f,
                     0.0f,
                     1.0f);
   }
}

static double MeasurePinRadiusM(const map::MapAnnotationStyle& style)
{
   return std::max(style.strokeWidthM.value() * 1.25, 1200.0);
}

namespace
{
static void
AppendRoundFreehandStroke(std::vector<float>&                    fillOut,
                          std::vector<float>&                    strokeOut,
                          const std::vector<common::Coordinate>& pts,
                          const map::MapAnnotationStyle&         style,
                          const ::GeographicLib::Geodesic&       geod)
{
   if (pts.size() >= 2)
   {
      const auto dash = DashAttribs(style);
      // Match live preview: segment stroke + round end caps. GEOS buffer+earcut
      // left holes ("cuts") and sometimes huge invalid fills on thick brushes.
      AppendPolylineStroke(strokeOut,
                           pts,
                           false,
                           style.strokeWidthM,
                           style.strokeColor,
                           geod,
                           dash.first,
                           dash.second);

      const double radiusM = style.strokeWidthM.value() * 0.5;
      AppendFilledGeoDisk(fillOut, pts.front(), radiusM, style.strokeColor);
      AppendFilledGeoDisk(fillOut, pts.back(), radiusM, style.strokeColor);
   }
   else if (pts.size() == 1)
   {
      const double radiusM = style.strokeWidthM.value() * 0.5;
      AppendFilledGeoDisk(fillOut, pts.front(), radiusM, style.strokeColor);
   }
}
} // namespace

struct PickSegment
{
   std::uint64_t                 id;
   common::Coordinate            a;
   common::Coordinate            b;
   units::length::meters<double> halfStrokeM {};
};

struct PickCircle
{
   std::uint64_t                 id;
   common::Coordinate            center;
   double                        radiusM {};
   units::length::meters<double> halfStrokeM {};
};

struct CommittedObjectGeometry
{
   std::vector<float>            strokeVertices_ {};
   std::vector<float>            fillVertices_ {};
   std::vector<PickSegment>      pickSegments_ {};
   std::vector<PickCircle>       pickCircles_ {};
   bool                          pickBoundsValid_ {false};
   double                        pickMinLat_ {0.0};
   double                        pickMaxLat_ {0.0};
   double                        pickMinLon_ {0.0};
   double                        pickMaxLon_ {0.0};
   units::length::meters<double> pickHalfStrokeM_ {};
};

struct PickObjectEntry
{
   std::uint64_t                 id {};
   double                        minLat_ {0.0};
   double                        maxLat_ {0.0};
   double                        minLon_ {0.0};
   double                        maxLon_ {0.0};
   units::length::meters<double> halfStrokeM_ {};
   std::vector<PickSegment>      segments_ {};
   std::vector<PickCircle>       circles_ {};
};

// Flat-earth bbox pad; same CONUS-scale assumption as segment pick distance.
static void ExpandPickBounds(CommittedObjectGeometry&      geom,
                             const common::Coordinate&     c,
                             units::length::meters<double> halfStrokeM)
{
   const double halfM  = std::max(0.0, halfStrokeM.value());
   const double padLat = halfM / 111320.0;
   const double cosLat = std::max(
      0.2, std::cos(c.latitude_ * (std::numbers::pi_v<double> / 180.0)));
   const double padLon = halfM / (111320.0 * cosLat);

   if (!geom.pickBoundsValid_)
   {
      geom.pickBoundsValid_ = true;
      geom.pickHalfStrokeM_ = halfStrokeM;
      geom.pickMinLat_      = c.latitude_ - padLat;
      geom.pickMaxLat_      = c.latitude_ + padLat;
      geom.pickMinLon_      = c.longitude_ - padLon;
      geom.pickMaxLon_      = c.longitude_ + padLon;
      return;
   }

   if (halfStrokeM > geom.pickHalfStrokeM_)
   {
      geom.pickHalfStrokeM_ = halfStrokeM;
   }
   geom.pickMinLat_ = std::min(geom.pickMinLat_, c.latitude_ - padLat);
   geom.pickMaxLat_ = std::max(geom.pickMaxLat_, c.latitude_ + padLat);
   geom.pickMinLon_ = std::min(geom.pickMinLon_, c.longitude_ - padLon);
   geom.pickMaxLon_ = std::max(geom.pickMaxLon_, c.longitude_ + padLon);
}

static void
AppendPolylinePickSegments(CommittedObjectGeometry&               geom,
                           std::uint64_t                          id,
                           const std::vector<common::Coordinate>& pts,
                           units::length::meters<double>          halfStrokeM,
                           bool                                   coarsePick)
{
   if (pts.size() < 2)
   {
      return;
   }

   constexpr std::size_t kMaxPickSegments = 64;
   const std::size_t     numSegments      = pts.size() - 1;
   const double          maxChordM = std::max(halfStrokeM.value() * 2.0, 5.0);

   auto emitSegment =
      [&](const common::Coordinate& a, const common::Coordinate& b)
   {
      geom.pickSegments_.push_back(
         PickSegment {.id = id, .a = a, .b = b, .halfStrokeM = halfStrokeM});
      ExpandPickBounds(geom, a, halfStrokeM);
      ExpandPickBounds(geom, b, halfStrokeM);
   };

   if (!coarsePick || numSegments <= kMaxPickSegments)
   {
      for (std::size_t i = 0; i < numSegments; ++i)
      {
         emitSegment(pts[i], pts[i + 1]);
      }
      return;
   }

   double pathLenM {0.0};
   for (std::size_t i = 0; i < numSegments; ++i)
   {
      pathLenM += util::GeographicLib::GetDistance(pts[i].latitude_,
                                                   pts[i].longitude_,
                                                   pts[i + 1].latitude_,
                                                   pts[i + 1].longitude_)
                     .value();
   }

   const std::size_t pickSegmentCount = std::clamp<std::size_t>(
      static_cast<std::size_t>(std::ceil(pathLenM / maxChordM)),
      static_cast<std::size_t>(1),
      kMaxPickSegments);

   std::vector<common::Coordinate> chain;
   chain.reserve(pickSegmentCount + 1);
   chain.push_back(pts.front());
   for (std::size_t k = 1; k < pickSegmentCount; ++k)
   {
      const std::size_t idx = (k * numSegments) / pickSegmentCount;
      chain.push_back(pts[idx]);
   }
   chain.push_back(pts.back());

   for (std::size_t i = 0; i + 1 < chain.size(); ++i)
   {
      emitSegment(chain[i], chain[i + 1]);
   }
}

static PickObjectEntry TakePickEntry(std::uint64_t            id,
                                     CommittedObjectGeometry& geom)
{
   PickObjectEntry entry {};
   entry.id              = id;
   entry.minLat_         = geom.pickMinLat_;
   entry.maxLat_         = geom.pickMaxLat_;
   entry.minLon_         = geom.pickMinLon_;
   entry.maxLon_         = geom.pickMaxLon_;
   entry.halfStrokeM_    = geom.pickHalfStrokeM_;
   entry.segments_       = std::move(geom.pickSegments_);
   entry.circles_        = std::move(geom.pickCircles_);
   geom.pickBoundsValid_ = false;
   return entry;
}

class MapAnnotationsDrawItem::Impl
{
public:
   explicit Impl(std::shared_ptr<render::RenderContext> context,
                 map::MapAnnotationModel*               model) :
       context_ {std::move(context)}, model_ {model}
   {
   }

   std::shared_ptr<render::RenderContext> context_;
   map::MapAnnotationModel*               model_ {nullptr};

   std::vector<float>           modelStrokeVertices_ {};
   std::vector<float>           modelFillVertices_ {};
   std::vector<float>           previewStrokeVertices_ {};
   std::vector<float>           previewFillVertices_ {};
   std::vector<PickObjectEntry> pickObjects_ {};
   std::unordered_map<std::uint64_t, CommittedObjectGeometry> committedById_ {};
   std::vector<common::Coordinate>                            previewPts_ {};
   map::MapAnnotationStyle                                    previewStyle_ {};
   bool previewActive_ {false};
   bool previewRoundStroke_ {false};
   /** When true with @c previewRoundStroke_, preview uses committed GEOS mesh.
    */
   bool previewCommittedRoundMesh_ {false};
   enum class PreviewShape : std::uint8_t
   {
      Polyline,
      Circle,
      Rectangle
   };
   PreviewShape       previewShape_ {PreviewShape::Polyline};
   common::Coordinate previewCircleCenter_ {};
   double             previewCircleRadiusM_ {0.0};
   common::Coordinate previewRectCorner1_ {};
   common::Coordinate previewRectCorner2_ {};

   std::uint32_t strokeModelCount_ {0};
   std::uint32_t strokePreviewCount_ {0};
   std::uint32_t fillModelCount_ {0};
   std::uint32_t fillPreviewCount_ {0};

   bool gpuModelDirty_ {true};
   bool gpuPreviewDirty_ {true};

   // Screen-space verts rebuilt only when model/preview or map camera moves.
   std::vector<float> cachedScreenVertices_ {};
   glm::mat4          cachedMapMatrix_ {1.0f};
   glm::vec2          cachedOrigin_ {0.0f, 0.0f};
   bool               screenCacheValid_ {false};

   void InvalidateScreenCache() { screenCacheValid_ = false; }

   void RebuildCommittedGeometry();
   void FlattenModelVertices();
   void RebuildPreviewGeometry();
};

MapAnnotationsDrawItem::MapAnnotationsDrawItem(
   std::shared_ptr<render::RenderContext> context,
   map::MapAnnotationModel*               model) :
    DrawItem(), p(std::make_unique<Impl>(std::move(context), model))
{
}
MapAnnotationsDrawItem::~MapAnnotationsDrawItem() = default;

void MapAnnotationsDrawItem::Impl::RebuildCommittedGeometry()
{
   committedById_.clear();
   pickObjects_.clear();
   const auto& geod = util::GeographicLib::DefaultGeodesic();

   auto handleObject =
      [&](const map::MapAnnotationObject& obj, CommittedObjectGeometry& geom)
   {
      std::visit(
         [&](auto&& arg)
         {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, map::MapAnnotationPolyline>)
            {
               if (arg.points.empty() ||
                   (arg.points.size() < 2 && !arg.roundStroke))
               {
                  return;
               }
               const auto dash = DashAttribs(obj.style);
               if (arg.roundStroke)
               {
                  AppendRoundFreehandStroke(geom.fillVertices_,
                                            geom.strokeVertices_,
                                            arg.points,
                                            obj.style,
                                            geod);
               }
               else
               {
                  AppendPolylineStroke(geom.strokeVertices_,
                                       arg.points,
                                       false,
                                       obj.style.strokeWidthM,
                                       obj.style.strokeColor,
                                       geod,
                                       dash.first,
                                       dash.second);
               }
               const auto halfStroke = obj.style.strokeWidthM * 0.5;
               if (arg.points.size() >= 2)
               {
                  AppendPolylinePickSegments(
                     geom, obj.id, arg.points, halfStroke, arg.roundStroke);
               }
               else if (arg.roundStroke && arg.points.size() == 1)
               {
                  const PickCircle circle {
                     .id          = obj.id,
                     .center      = arg.points[0],
                     .radiusM     = obj.style.strokeWidthM.value() * 0.5,
                     .halfStrokeM = halfStroke};
                  geom.pickCircles_.push_back(circle);
                  ExpandPickBounds(geom, circle.center, halfStroke);
               }
            }
            else if constexpr (std::is_same_v<T, map::MapAnnotationCircle>)
            {
               const auto dash = DashAttribs(obj.style);
               if (obj.style.polygonFill)
               {
                  const double fillRadiusM =
                     InsetFillRadiusM(arg.radiusMeters, obj.style.strokeWidthM);
                  if (fillRadiusM > 0.0)
                  {
                     AppendFilledGeoDisk(geom.fillVertices_,
                                         arg.center,
                                         fillRadiusM,
                                         ShapeFillRgba(obj.style));
                  }
               }
               AppendCircleStroke(geom.strokeVertices_,
                                  arg.center,
                                  arg.radiusMeters,
                                  obj.style.strokeWidthM,
                                  obj.style.strokeColor,
                                  geod,
                                  dash.first,
                                  dash.second);
               const auto halfStroke = obj.style.strokeWidthM * 0.5;
               geom.pickCircles_.push_back(
                  PickCircle {.id          = obj.id,
                              .center      = arg.center,
                              .radiusM     = arg.radiusMeters,
                              .halfStrokeM = halfStroke});
               ExpandPickBounds(geom,
                                arg.center,
                                units::length::meters<double> {
                                   arg.radiusMeters + halfStroke.value()});
            }
            else if constexpr (std::is_same_v<T, map::MapAnnotationRectangle>)
            {
               AppendRectangleStrokeAndFill(geom.strokeVertices_,
                                            geom.fillVertices_,
                                            arg.corner1,
                                            arg.corner2,
                                            obj.style,
                                            geod);
               const double minLat =
                  std::min(arg.corner1.latitude_, arg.corner2.latitude_);
               const double maxLat =
                  std::max(arg.corner1.latitude_, arg.corner2.latitude_);
               const double minLon =
                  std::min(arg.corner1.longitude_, arg.corner2.longitude_);
               const double maxLon =
                  std::max(arg.corner1.longitude_, arg.corner2.longitude_);
               const common::Coordinate ll {minLat, minLon};
               const common::Coordinate lr {minLat, maxLon};
               const common::Coordinate ur {maxLat, maxLon};
               const common::Coordinate ul {maxLat, minLon};
               geom.pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = ll,
                               .b           = lr,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               geom.pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = lr,
                               .b           = ur,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               geom.pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = ur,
                               .b           = ul,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               geom.pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = ul,
                               .b           = ll,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               const auto halfStroke = obj.style.strokeWidthM * 0.5;
               ExpandPickBounds(geom, ll, halfStroke);
               ExpandPickBounds(geom, lr, halfStroke);
               ExpandPickBounds(geom, ur, halfStroke);
               ExpandPickBounds(geom, ul, halfStroke);
            }
            else if constexpr (std::is_same_v<T, map::MapAnnotationMeasure>)
            {
               const double pinRadiusM = MeasurePinRadiusM(obj.style);
               const auto   dash       = DashAttribs(obj.style);
               const std::vector<common::Coordinate> seg = {arg.a, arg.b};
               AppendPolylineStroke(
                  geom.strokeVertices_,
                  seg,
                  false,
                  units::length::meters<double> {pinRadiusM * 0.8},
                  obj.style.strokeColor,
                  geod,
                  dash.first,
                  dash.second);
               AppendFilledGeoDisk(
                  geom.fillVertices_, arg.a, pinRadiusM, obj.style.strokeColor);
               AppendFilledGeoDisk(
                  geom.fillVertices_, arg.b, pinRadiusM, obj.style.strokeColor);
               AppendFilledGeoDisk(geom.fillVertices_,
                                   arg.a,
                                   pinRadiusM * 0.38,
                                   {1.0f, 1.0f, 1.0f, 0.95f});
               AppendFilledGeoDisk(geom.fillVertices_,
                                   arg.b,
                                   pinRadiusM * 0.38,
                                   {1.0f, 1.0f, 1.0f, 0.95f});
               geom.pickCircles_.push_back(PickCircle {
                  .id          = obj.id,
                  .center      = arg.a,
                  .radiusM     = pinRadiusM,
                  .halfStrokeM = units::length::meters<double> {0.0}});
               geom.pickCircles_.push_back(PickCircle {
                  .id          = obj.id,
                  .center      = arg.b,
                  .radiusM     = pinRadiusM,
                  .halfStrokeM = units::length::meters<double> {0.0}});
               geom.pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = arg.a,
                               .b           = arg.b,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               const auto pinHalf = units::length::meters<double> {pinRadiusM};
               ExpandPickBounds(geom, arg.a, pinHalf);
               ExpandPickBounds(geom, arg.b, pinHalf);
               ExpandPickBounds(geom, arg.a, obj.style.strokeWidthM * 0.5);
               ExpandPickBounds(geom, arg.b, obj.style.strokeWidthM * 0.5);
            }
         },
         obj.payload);
   };

   if (model_ != nullptr)
   {
      model_->Read(
         [&](const std::vector<map::MapAnnotationObject>& objs)
         {
            for (const auto& o : objs)
            {
               CommittedObjectGeometry geom;
               handleObject(o, geom);
               pickObjects_.push_back(TakePickEntry(o.id, geom));
               committedById_.emplace(o.id, std::move(geom));
            }
         });
   }

   FlattenModelVertices();
}

void MapAnnotationsDrawItem::Impl::FlattenModelVertices()
{
   modelStrokeVertices_.clear();
   modelFillVertices_.clear();

   auto appendRender = [this](const CommittedObjectGeometry& geom)
   {
      modelStrokeVertices_.insert(modelStrokeVertices_.end(),
                                  geom.strokeVertices_.begin(),
                                  geom.strokeVertices_.end());
      modelFillVertices_.insert(modelFillVertices_.end(),
                                geom.fillVertices_.begin(),
                                geom.fillVertices_.end());
   };

   if (model_ != nullptr)
   {
      model_->Read(
         [&](const std::vector<map::MapAnnotationObject>& objs)
         {
            for (const auto& o : objs)
            {
               const auto it = committedById_.find(o.id);
               if (it == committedById_.end())
               {
                  continue;
               }
               appendRender(it->second);
            }
         });
   }
   else
   {
      for (const auto& [_, geom] : committedById_)
      {
         appendRender(geom);
      }
   }

   strokeModelCount_ = static_cast<std::uint32_t>(modelStrokeVertices_.size() /
                                                  kFloatsPerVertex);
   fillModelCount_ =
      static_cast<std::uint32_t>(modelFillVertices_.size() / kFloatsPerVertex);
   gpuModelDirty_ = true;
   InvalidateScreenCache();
}

void MapAnnotationsDrawItem::Impl::RebuildPreviewGeometry()
{
   previewStrokeVertices_.clear();
   previewFillVertices_.clear();
   strokePreviewCount_ = 0;
   fillPreviewCount_   = 0;

   if (!previewActive_)
   {
      gpuPreviewDirty_ = true;
      InvalidateScreenCache();
      return;
   }

   const auto& geod = util::GeographicLib::DefaultGeodesic();
   if (previewShape_ == PreviewShape::Circle)
   {
      if (previewCircleRadiusM_ > 0.0)
      {
         if (previewStyle_.polygonFill)
         {
            const double fillRadiusM = InsetFillRadiusM(
               previewCircleRadiusM_, previewStyle_.strokeWidthM);
            if (fillRadiusM > 0.0)
            {
               AppendFilledGeoDisk(previewFillVertices_,
                                   previewCircleCenter_,
                                   fillRadiusM,
                                   ShapeFillRgba(previewStyle_));
            }
         }
         const auto dash = DashAttribs(previewStyle_);
         AppendCircleStroke(previewStrokeVertices_,
                            previewCircleCenter_,
                            previewCircleRadiusM_,
                            previewStyle_.strokeWidthM,
                            previewStyle_.strokeColor,
                            geod,
                            dash.first,
                            dash.second);
      }
   }
   else if (previewShape_ == PreviewShape::Rectangle)
   {
      AppendRectangleStrokeAndFill(previewStrokeVertices_,
                                   previewFillVertices_,
                                   previewRectCorner1_,
                                   previewRectCorner2_,
                                   previewStyle_,
                                   geod);
   }
   else if (previewRoundStroke_)
   {
      if (previewPts_.empty())
      {
         gpuPreviewDirty_ = true;
         InvalidateScreenCache();
         return;
      }
      if (previewCommittedRoundMesh_)
      {
         AppendRoundFreehandStroke(previewFillVertices_,
                                   previewStrokeVertices_,
                                   previewPts_,
                                   previewStyle_,
                                   geod);
      }
      else if (previewPts_.size() >= 2)
      {
         const auto dash = DashAttribs(previewStyle_);
         AppendPolylineStroke(previewStrokeVertices_,
                              previewPts_,
                              false,
                              previewStyle_.strokeWidthM,
                              previewStyle_.strokeColor,
                              geod,
                              dash.first,
                              dash.second);
      }
      else if (previewPts_.size() == 1)
      {
         const double radiusM = previewStyle_.strokeWidthM.value() * 0.5;
         AppendFilledGeoDisk(previewFillVertices_,
                             previewPts_.front(),
                             radiusM,
                             previewStyle_.strokeColor);
      }
   }
   else
   {
      if (previewPts_.size() < 2)
      {
         gpuPreviewDirty_ = true;
         InvalidateScreenCache();
         return;
      }
      const auto dash = DashAttribs(previewStyle_);
      AppendPolylineStroke(previewStrokeVertices_,
                           previewPts_,
                           false,
                           previewStyle_.strokeWidthM,
                           previewStyle_.strokeColor,
                           geod,
                           dash.first,
                           dash.second);
   }

   strokePreviewCount_ = static_cast<std::uint32_t>(
      previewStrokeVertices_.size() / kFloatsPerVertex);
   fillPreviewCount_ = static_cast<std::uint32_t>(previewFillVertices_.size() /
                                                  kFloatsPerVertex);
   gpuPreviewDirty_  = true;
   InvalidateScreenCache();
}

void MapAnnotationsDrawItem::Initialize()
{
   Rebuild();
}

void MapAnnotationsDrawItem::Deinitialize() {}

void MapAnnotationsDrawItem::SetPreviewPolyline(
   const std::vector<common::Coordinate>& pts,
   const map::MapAnnotationStyle&         style,
   bool                                   roundStroke,
   bool                                   committedRoundMeshPreview)
{
   p->previewPts_                = pts;
   p->previewStyle_              = style;
   p->previewRoundStroke_        = roundStroke;
   p->previewCommittedRoundMesh_ = committedRoundMeshPreview;
   p->previewShape_              = Impl::PreviewShape::Polyline;
   p->previewActive_             = true;
   p->RebuildPreviewGeometry();
}

void MapAnnotationsDrawItem::SetPreviewCircle(
   const common::Coordinate&      center,
   double                         radiusMeters,
   const map::MapAnnotationStyle& style)
{
   p->previewStyle_              = style;
   p->previewShape_              = Impl::PreviewShape::Circle;
   p->previewCircleCenter_       = center;
   p->previewCircleRadiusM_      = radiusMeters;
   p->previewRoundStroke_        = false;
   p->previewCommittedRoundMesh_ = false;
   p->previewPts_.clear();
   p->previewActive_ = true;
   p->RebuildPreviewGeometry();
}

void MapAnnotationsDrawItem::SetPreviewRectangle(
   const common::Coordinate&      corner1,
   const common::Coordinate&      corner2,
   const map::MapAnnotationStyle& style)
{
   p->previewStyle_              = style;
   p->previewShape_              = Impl::PreviewShape::Rectangle;
   p->previewRectCorner1_        = corner1;
   p->previewRectCorner2_        = corner2;
   p->previewRoundStroke_        = false;
   p->previewCommittedRoundMesh_ = false;
   p->previewPts_.clear();
   p->previewActive_ = true;
   p->RebuildPreviewGeometry();
}

void MapAnnotationsDrawItem::ClearPreview()
{
   p->previewActive_             = false;
   p->previewRoundStroke_        = false;
   p->previewCommittedRoundMesh_ = false;
   p->previewShape_              = Impl::PreviewShape::Polyline;
   p->previewCircleRadiusM_      = 0.0;
   p->previewPts_.clear();
   p->previewStrokeVertices_.clear();
   p->previewFillVertices_.clear();
   p->strokePreviewCount_ = 0;
   p->fillPreviewCount_   = 0;
   p->gpuPreviewDirty_    = true;
   p->InvalidateScreenCache();
}

void MapAnnotationsDrawItem::Rebuild()
{
   p->RebuildCommittedGeometry();
}

void MapAnnotationsDrawItem::RemoveCommittedObjects(
   const std::unordered_set<std::uint64_t>& ids)
{
   if (ids.empty())
   {
      return;
   }

   for (const std::uint64_t id : ids)
   {
      p->committedById_.erase(id);
   }
   std::erase_if(p->pickObjects_,
                 [&ids](const PickObjectEntry& entry)
                 { return ids.contains(entry.id); });
   p->FlattenModelVertices();
}

namespace
{
// Cross-track distance on the spheroid via local azimuth/length; adequate for
// CONUS-scale annotations (not polar geodesic line-of-closest-approach).
double DistancePointToSegmentM(const common::Coordinate& p,
                               const common::Coordinate& a,
                               const common::Coordinate& b)
{
   const auto& geod = util::GeographicLib::DefaultGeodesic();
   double      s12 {};
   double      azi1 {};
   double      azi2 {};
   geod.Inverse(
      a.latitude_, a.longitude_, b.latitude_, b.longitude_, s12, azi1, azi2);
   if (s12 <= 0.0)
   {
      return util::GeographicLib::GetDistance(
                a.latitude_, a.longitude_, p.latitude_, p.longitude_)
         .value();
   }

   double s13 {};
   double azi13 {};
   double azi31 {};
   geod.Inverse(
      a.latitude_, a.longitude_, p.latitude_, p.longitude_, s13, azi13, azi31);

   double aziDeltaDeg = azi13 - azi1;
   while (aziDeltaDeg > 180.0)
   {
      aziDeltaDeg -= 360.0;
   }
   while (aziDeltaDeg < -180.0)
   {
      aziDeltaDeg += 360.0;
   }
   const double aziDiffRad = aziDeltaDeg * (std::numbers::pi_v<double> / 180.0);
   const double crossM     = std::abs(s13 * std::sin(aziDiffRad));
   const double alongM     = s13 * std::cos(aziDiffRad);
   if (alongM < 0.0 || alongM > s12)
   {
      return std::min(util::GeographicLib::GetDistance(
                         a.latitude_, a.longitude_, p.latitude_, p.longitude_)
                         .value(),
                      util::GeographicLib::GetDistance(
                         b.latitude_, b.longitude_, p.latitude_, p.longitude_)
                         .value());
   }
   return crossM;
}

// Flat-earth bbox test; adequate for CONUS-scale annotations.
bool MouseNearPickObject(const common::Coordinate& mouseGeo,
                         const PickObjectEntry&    entry,
                         double                    extraHalfM)
{
   const double pad    = entry.halfStrokeM_.value() + extraHalfM;
   const double midLat = (entry.minLat_ + entry.maxLat_) * 0.5;
   const double padLat = pad / 111320.0;
   const double cosLat =
      std::max(0.2, std::cos(midLat * (std::numbers::pi_v<double> / 180.0)));
   const double padLon = pad / (111320.0 * cosLat);

   return mouseGeo.latitude_ >= entry.minLat_ - padLat &&
          mouseGeo.latitude_ <= entry.maxLat_ + padLat &&
          mouseGeo.longitude_ >= entry.minLon_ - padLon &&
          mouseGeo.longitude_ <= entry.maxLon_ + padLon;
}
} // namespace

std::vector<std::uint64_t> MapAnnotationsDrawItem::PickObjects(
   const common::Coordinate&     mouseGeo,
   units::length::meters<double> pickExtraHalfWidthM) const
{
   const double extraHalfM = std::max(0.0, pickExtraHalfWidthM.value());

   std::unordered_set<std::uint64_t> hitIds;
   hitIds.reserve(8);

   for (const auto& object : p->pickObjects_)
   {
      if (!MouseNearPickObject(mouseGeo, object, extraHalfM))
      {
         continue;
      }

      for (const auto& seg : object.segments_)
      {
         const double pickRadiusM = seg.halfStrokeM.value() + extraHalfM;
         const double distM = DistancePointToSegmentM(mouseGeo, seg.a, seg.b);
         if (distM <= pickRadiusM)
         {
            hitIds.insert(seg.id);
         }
      }

      for (const auto& c : object.circles_)
      {
         const double distM =
            util::GeographicLib::GetDistance(c.center.latitude_,
                                             c.center.longitude_,
                                             mouseGeo.latitude_,
                                             mouseGeo.longitude_)
               .value();
         const double pickRadiusM = c.halfStrokeM.value() + extraHalfM;
         if (distM <= c.radiusM + pickRadiusM)
         {
            hitIds.insert(c.id);
            continue;
         }
         const double ringDist = std::abs(distM - c.radiusM);
         if (ringDist <= pickRadiusM)
         {
            hitIds.insert(c.id);
         }
      }
   }

   std::vector<std::uint64_t> ids {hitIds.begin(), hitIds.end()};
   std::sort(ids.begin(), ids.end());
   return ids;
}

void MapAnnotationsDrawItem::Render(
   const QMapLibre::CustomLayerRenderParameters& /* params */)
{
}

void MapAnnotationsDrawItem::RenderVulkan(
   QRhiCommandBuffer*                            commandBuffer,
   render::RhiVulkanOverlayResources&            resources,
   const QMapLibre::CustomLayerRenderParameters& params,
   bool /* textureAtlasChanged */)
{
   const glm::vec2 mapScale = util::maplibre::GetMapScale(params);
   glm::mat4       mapMatrix =
      glm::scale(glm::mat4 {1.0f}, glm::vec3(mapScale.x, -mapScale.y, 1.0f));
   mapMatrix =
      glm::rotate(mapMatrix,
                  glm::radians<float>(static_cast<float>(params.bearing)),
                  glm::vec3 {0.0f, 0.0f, 1.0f});
   const glm::vec2 origin {static_cast<float>(params.latitude),
                           static_cast<float>(params.longitude)};

   const bool cameraChanged = !p->screenCacheValid_ ||
                              p->cachedOrigin_ != origin ||
                              p->cachedMapMatrix_ != mapMatrix;

   if (cameraChanged || p->gpuModelDirty_ || p->gpuPreviewDirty_)
   {
      // Fill verts first, then stroke, in one upload/draw. Opaque stroke
      // fragments (color.frag a>=0.45) replace on top of fills already
      // rasterized earlier in the same draw.
      p->cachedScreenVertices_.clear();
      const auto appendBuilt =
         [&](const std::vector<float>& source, std::uint32_t vertexCount)
      {
         if (vertexCount == 0)
         {
            return;
         }
         std::vector<float> built = BuildColoredAnnotationVertices(
            source, vertexCount, mapMatrix, origin);
         p->cachedScreenVertices_.insert(
            p->cachedScreenVertices_.end(), built.begin(), built.end());
      };

      appendBuilt(p->modelFillVertices_, p->fillModelCount_);
      appendBuilt(p->previewFillVertices_, p->fillPreviewCount_);
      appendBuilt(p->modelStrokeVertices_, p->strokeModelCount_);
      appendBuilt(p->previewStrokeVertices_, p->strokePreviewCount_);

      p->cachedMapMatrix_  = mapMatrix;
      p->cachedOrigin_     = origin;
      p->screenCacheValid_ = true;
      p->gpuModelDirty_    = false;
      p->gpuPreviewDirty_  = false;
   }

   if (!p->cachedScreenVertices_.empty())
   {
      resources.coloredGeometry.Render(commandBuffer,
                                       glm::mat4 {1.0f},
                                       p->cachedScreenVertices_,
                                       p->cachedScreenVertices_.size() / 7,
                                       resources.resourceBatch,
                                       resources.phase);
   }
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,modernize-use-auto,cppcoreguidelines-pro-type-cstyle-cast,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-pro-bounds-pointer-arithmetic,performance-no-int-to-ptr)
} // namespace scwx::qt::draw
