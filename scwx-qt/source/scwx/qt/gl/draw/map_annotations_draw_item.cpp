#include <scwx/qt/gl/draw/map_annotations_draw_item.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/qt/map/map_annotation_model.hpp>
#include <scwx/qt/util/geographic_lib.hpp>
#include <scwx/qt/util/maplibre.hpp>
#include <scwx/util/logger.hpp>

#if !defined(__APPLE__)
#   include <GL/glu.h>
#else
#   include <OpenGL/glu.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <deque>
#include <exception>
#include <numbers>
#include <numeric>
#include <type_traits>

#include <GeographicLib/Geodesic.hpp>
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LineString.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/Polygon.h>
#include <geos/operation/buffer/BufferOp.h>
#include <geos/operation/buffer/BufferParameters.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(_WIN32) || defined(__APPLE__)
typedef void (*_GLUfuncptr)(void);
#endif

// Geometry/render math here intentionally uses tuned constants and OpenGL/GLU
// interop patterns that clang-tidy flags noisily without improving clarity.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,modernize-use-auto,cppcoreguidelines-pro-type-cstyle-cast,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-pro-bounds-pointer-arithmetic,performance-no-int-to-ptr)
namespace scwx::qt::gl::draw
{

static const std::string logPrefix_ =
   "scwx::qt::gl::draw::map_annotations_draw_item";
static const auto logger_ = scwx::util::Logger::Create(logPrefix_);

static constexpr int kFloatsPerVertex = 11;

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
   const std::size_t n = pts.size();
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
      GeoStrokeSegmentCorners(pts[n - 1],
                              pts[0],
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
      const common::Coordinate& a = closed ? pts[s % n] : pts[s];
      const common::Coordinate& b = closed ? pts[(s + 1) % n] : pts[s + 1];

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
            closed ? pts[(s + n - 1) % n] : pts[s - 1];
         const common::Coordinate& p1 = closed ? pts[s % n] : pts[s];
         const common::Coordinate& p2 = closed ? pts[(s + 1) % n] : pts[s + 1];

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

static void AppendCircleStroke(std::vector<float>&              out,
                               const common::Coordinate&        center,
                               double                           radiusM,
                               units::length::meters<double>    strokeWidthM,
                               const std::array<float, 4>&      rgba,
                               const ::GeographicLib::Geodesic& geod,
                               float                            dashPeriodM,
                               float                            dashDuty)
{
   constexpr int                   kSegments = 72;
   std::vector<common::Coordinate> ring;
   ring.reserve(static_cast<std::size_t>(kSegments) + 1);
   for (int i = 0; i <= kSegments; ++i)
   {
      const double az =
         360.0 * static_cast<double>(i) / static_cast<double>(kSegments);
      ring.push_back(util::GeographicLib::GetCoordinate(
         center,
         units::angle::degrees<double> {az},
         units::length::meters<double> {radiusM}));
   }
   AppendPolylineStroke(
      out, ring, true, strokeWidthM, rgba, geod, dashPeriodM, dashDuty);
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

   std::vector<common::Coordinate> ring = {
      {minLat, minLon},
      {minLat, maxLon},
      {maxLat, maxLon},
      {maxLat, minLon},
   };

   const auto dash = DashAttribs(style);
   AppendPolylineStroke(strokeOut,
                        ring,
                        true,
                        style.strokeWidthM,
                        style.strokeColor,
                        geod,
                        dash.first,
                        dash.second);

   if (style.polygonFill && ring.size() >= 3)
   {
      common::Coordinate centroid {0.0, 0.0};
      for (const auto& v : ring)
      {
         centroid.latitude_ += v.latitude_;
         centroid.longitude_ += v.longitude_;
      }
      centroid.latitude_ /= static_cast<double>(ring.size());
      centroid.longitude_ /= static_cast<double>(ring.size());

      const std::array<float, 4> fillRgba = style.fillColor;
      const std::size_t          n        = ring.size();
      for (std::size_t i = 0; i < n; ++i)
      {
         const std::size_t j = (i + 1) % n;
         AppendTriangle(
            fillOut, centroid, ring[i], ring[j], fillRgba, 0.0f, 0.0f, 1.0f);
      }
   }
}

static constexpr int kRoundBrushDiskSegments = 22;

static void AppendFilledGeoDisk(std::vector<float>&         fillOut,
                                const common::Coordinate&   center,
                                double                      radiusM,
                                const std::array<float, 4>& rgba)
{
   if (radiusM <= 0.0)
   {
      return;
   }
   std::vector<common::Coordinate> ring;
   ring.reserve(static_cast<std::size_t>(kRoundBrushDiskSegments) + 1);
   for (int i = 0; i <= kRoundBrushDiskSegments; ++i)
   {
      const double az = 360.0 * static_cast<double>(i) /
                        static_cast<double>(kRoundBrushDiskSegments);
      ring.push_back(util::GeographicLib::GetCoordinate(
         center,
         units::angle::degrees<double> {az},
         units::length::meters<double> {radiusM}));
   }
   for (int i = 0; i < kRoundBrushDiskSegments; ++i)
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
static constexpr double kEarthRadiusM = 6371000.0;

struct MeterVertex
{
   double x;
   double y;
};

static void PolylineBBoxCenter(const std::vector<common::Coordinate>& pts,
                               double&                                refLat,
                               double&                                refLon)
{
   double minLat = pts.front().latitude_;
   double maxLat = minLat;
   double minLon = pts.front().longitude_;
   double maxLon = minLon;
   for (std::size_t i = 1; i < pts.size(); ++i)
   {
      minLat = std::min(minLat, pts[i].latitude_);
      maxLat = std::max(maxLat, pts[i].latitude_);
      minLon = std::min(minLon, pts[i].longitude_);
      maxLon = std::max(maxLon, pts[i].longitude_);
   }
   refLat = 0.5 * (minLat + maxLat);
   refLon = 0.5 * (minLon + maxLon);
}

static void LatLonToEnuMeters(double  lat,
                              double  lon,
                              double  refLat,
                              double  refLon,
                              double& eastM,
                              double& northM)
{
   constexpr double kDeg   = std::numbers::pi / 180.0;
   const double     latRad = refLat * kDeg;
   eastM  = kEarthRadiusM * std::cos(latRad) * (lon - refLon) * kDeg;
   northM = kEarthRadiusM * (lat - refLat) * kDeg;
}

static common::Coordinate
EnuMetersToLatLon(double eastM, double northM, double refLat, double refLon)
{
   constexpr double kDeg   = std::numbers::pi / 180.0;
   const double     latRad = refLat * kDeg;
   const double     lat    = refLat + (northM / kEarthRadiusM) / kDeg;
   const double     lon =
      refLon + (eastM / (kEarthRadiusM * std::cos(latRad))) / kDeg;
   return {lat, lon};
}

class StrokePolygonTessellator
{
public:
   StrokePolygonTessellator() : tessellator_ {gluNewTess()}
   {
      gluTessCallback(
         tessellator_, GLU_TESS_COMBINE_DATA, (_GLUfuncptr) &CombineCallback);
      gluTessCallback(
         tessellator_, GLU_TESS_VERTEX_DATA, (_GLUfuncptr) &VertexCallback);
      gluTessCallback(tessellator_, GLU_TESS_EDGE_FLAG, []() {});
      gluTessCallback(
         tessellator_, GLU_TESS_ERROR, (_GLUfuncptr) &ErrorCallback);
   }

   ~StrokePolygonTessellator() { gluDeleteTess(tessellator_); }
   StrokePolygonTessellator(const StrokePolygonTessellator&) = delete;
   StrokePolygonTessellator&
   operator=(const StrokePolygonTessellator&)                      = delete;
   StrokePolygonTessellator(StrokePolygonTessellator&&)            = delete;
   StrokePolygonTessellator& operator=(StrokePolygonTessellator&&) = delete;

   bool TessellateGeometry(const geos::geom::Geometry& geometry,
                           std::vector<float>&         fillOut,
                           double                      refLat,
                           double                      refLon,
                           const std::array<float, 4>& rgba)
   {
      if (const auto* poly =
             dynamic_cast<const geos::geom::Polygon*>(&geometry))
      {
         return TessellatePolygon(*poly, fillOut, refLat, refLon, rgba);
      }
      if (const auto* mp =
             dynamic_cast<const geos::geom::MultiPolygon*>(&geometry))
      {
         bool any = false;
         for (std::size_t i = 0; i < mp->getNumGeometries(); ++i)
         {
            const auto* poly =
               dynamic_cast<const geos::geom::Polygon*>(mp->getGeometryN(i));
            if (poly != nullptr &&
                TessellatePolygon(*poly, fillOut, refLat, refLon, rgba))
            {
               any = true;
            }
         }
         return any;
      }
      return false;
   }

private:
   using TessVertexData = std::array<GLdouble, 3>;

   static void CombineCallback(GLdouble coords[3],
                               void* /*vertexData*/[4],
                               GLfloat /*weight*/[4],
                               void** outData,
                               void*  polygonData)
   {
      auto* self = static_cast<StrokePolygonTessellator*>(polygonData);
      auto& v    = self->combineBuffer_.emplace_back(
         TessVertexData {coords[0], coords[1], coords[2]});
      *outData = v.data();
   }

   static void VertexCallback(void* vertexData, void* polygonData)
   {
      auto* self = static_cast<StrokePolygonTessellator*>(polygonData);
      auto* v    = static_cast<GLdouble*>(vertexData);
      self->triangles_.push_back(MeterVertex {.x = v[0], .y = v[1]});
   }

   static void ErrorCallback(GLenum errorCode)
   {
      const GLubyte* msg = gluErrorString(errorCode);
      logger_->warn("GLU stroke tessellation error: {}",
                    msg != nullptr ? reinterpret_cast<const char*>(msg) :
                                     "unknown");
   }

   void Reset()
   {
      vertices_.clear();
      combineBuffer_.clear();
      triangles_.clear();
   }

   bool AddContour(const geos::geom::LineString* ring)
   {
      if (ring == nullptr)
      {
         return false;
      }
      std::unique_ptr<geos::geom::CoordinateSequence> cs =
         ring->getCoordinates();
      if (cs == nullptr || cs->size() < 4U)
      {
         return false;
      }

      std::size_t n = cs->size();
      if (cs->front() == cs->back())
      {
         --n;
      }
      if (n < 3U)
      {
         return false;
      }

      gluTessBeginContour(tessellator_);
      for (std::size_t i = 0; i < n; ++i)
      {
         const geos::geom::Coordinate& c = cs->getAt(i);
         auto& v = vertices_.emplace_back(TessVertexData {c.x, c.y, 0.0});
         gluTessVertex(tessellator_, v.data(), v.data());
      }
      gluTessEndContour(tessellator_);
      return true;
   }

   bool TessellatePolygon(const geos::geom::Polygon&  poly,
                          std::vector<float>&         fillOut,
                          double                      refLat,
                          double                      refLon,
                          const std::array<float, 4>& rgba)
   {
      Reset();

      gluTessBeginPolygon(tessellator_, this);
      const bool haveShell = AddContour(poly.getExteriorRing());
      for (std::size_t i = 0; i < poly.getNumInteriorRing(); ++i)
      {
         static_cast<void>(AddContour(poly.getInteriorRingN(i)));
      }
      gluTessEndPolygon(tessellator_);

      while (triangles_.size() % 3U != 0U)
      {
         triangles_.pop_back();
      }
      if (!haveShell || triangles_.empty())
      {
         return false;
      }

      for (std::size_t i = 0; i < triangles_.size(); i += 3)
      {
         const common::Coordinate c0 = EnuMetersToLatLon(
            triangles_[i + 0].x, triangles_[i + 0].y, refLat, refLon);
         const common::Coordinate c1 = EnuMetersToLatLon(
            triangles_[i + 1].x, triangles_[i + 1].y, refLat, refLon);
         const common::Coordinate c2 = EnuMetersToLatLon(
            triangles_[i + 2].x, triangles_[i + 2].y, refLat, refLon);
         AppendTriangle(fillOut, c0, c1, c2, rgba, 0.0f, 0.0f, 1.0f);
      }
      return true;
   }

   GLUtesselator*             tessellator_ {nullptr};
   std::deque<TessVertexData> vertices_ {};
   std::deque<TessVertexData> combineBuffer_ {};
   std::vector<MeterVertex>   triangles_ {};
};

static bool
TryAppendSolidRoundPolylineGeosFill(std::vector<float>& fillOut,
                                    const std::vector<common::Coordinate>& pts,
                                    units::length::meters<double> strokeWidthM,
                                    const std::array<float, 4>&   rgba)
{
   if (pts.size() < 2)
   {
      return false;
   }
   const double halfW = strokeWidthM.value() * 0.5;
   if (halfW <= 0.0)
   {
      return false;
   }

   double refLat {};
   double refLon {};
   PolylineBBoxCenter(pts, refLat, refLon);

   geos::geom::CoordinateSequence seq;
   for (const auto& p : pts)
   {
      double eastM {};
      double northM {};
      LatLonToEnuMeters(
         p.latitude_, p.longitude_, refLat, refLon, eastM, northM);
      seq.add(eastM, northM);
   }

   try
   {
      const auto& gf   = *geos::geom::GeometryFactory::getDefaultInstance();
      auto        line = gf.createLineString(seq);
      using BP         = geos::operation::buffer::BufferParameters;
      const BP bp(16U, BP::CAP_ROUND, BP::JOIN_ROUND, 5.0);
      geos::operation::buffer::BufferOp     op(line.get(), bp);
      std::unique_ptr<geos::geom::Geometry> buf(op.getResultGeometry(halfW));
      if (buf == nullptr || buf->isEmpty())
      {
         return false;
      }
      StrokePolygonTessellator tessellator;
      return tessellator.TessellateGeometry(
         *buf, fillOut, refLat, refLon, rgba);
   }
   catch (const std::exception& ex)
   {
      logger_->warn("GEOS round stroke failed: {}", ex.what());
      return false;
   }

   return false;
}
} // namespace

static void ConfigureAnnotationVaoForVbo(GLuint vao, GLuint vbo)
{
   glBindVertexArray(vao);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
   glVertexAttribPointer(
      0, 2, GL_FLOAT, GL_FALSE, kFloatsPerVertex * sizeof(float), nullptr);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(1,
                         4,
                         GL_FLOAT,
                         GL_FALSE,
                         kFloatsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(2 * sizeof(float)));
   glEnableVertexAttribArray(1);
   glVertexAttribPointer(2,
                         1,
                         GL_FLOAT,
                         GL_FALSE,
                         kFloatsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(6 * sizeof(float)));
   glEnableVertexAttribArray(2);
   glVertexAttribPointer(3,
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         kFloatsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(7 * sizeof(float)));
   glEnableVertexAttribArray(3);
   glVertexAttribPointer(4,
                         2,
                         GL_FLOAT,
                         GL_FALSE,
                         kFloatsPerVertex * sizeof(float),
                         reinterpret_cast<void*>(9 * sizeof(float)));
   glEnableVertexAttribArray(4);
   glBindVertexArray(0);
}

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

class MapAnnotationsDrawItem::Impl
{
public:
   explicit Impl(std::shared_ptr<GlContext> context,
                 map::MapAnnotationModel*   model) :
       context_ {std::move(context)}, model_ {model}
   {
   }

   std::shared_ptr<GlContext> context_;
   map::MapAnnotationModel*   model_ {nullptr};

   std::vector<float>              modelStrokeVertices_ {};
   std::vector<float>              modelFillVertices_ {};
   std::vector<float>              previewStrokeVertices_ {};
   std::vector<float>              previewFillVertices_ {};
   std::vector<PickSegment>        pickSegments_ {};
   std::vector<PickCircle>         pickCircles_ {};
   std::vector<common::Coordinate> previewPts_ {};
   map::MapAnnotationStyle         previewStyle_ {};
   bool                            previewActive_ {false};
   bool                            previewRoundStroke_ {false};

   std::shared_ptr<ShaderProgram> shader_ {nullptr};
   GLint                          uMapMatrixLoc_ {-1};
   GLint                          uOriginLoc_ {-1};
   GLint                          uHatchModeLoc_ {-1};

   GLuint  vaoModelStroke_ {0};
   GLuint  vboModelStroke_ {0};
   GLuint  vaoPreviewStroke_ {0};
   GLuint  vboPreviewStroke_ {0};
   GLsizei strokeModelCount_ {0};
   GLsizei strokePreviewCount_ {0};

   GLuint  vaoModelFill_ {0};
   GLuint  vboModelFill_ {0};
   GLuint  vaoPreviewFill_ {0};
   GLuint  vboPreviewFill_ {0};
   GLsizei fillModelCount_ {0};
   GLsizei fillPreviewCount_ {0};

   bool gpuModelDirty_ {true};
   bool gpuPreviewDirty_ {true};

   void RebuildCommittedGeometry();
   void RebuildPreviewGeometry();
};

MapAnnotationsDrawItem::MapAnnotationsDrawItem(
   std::shared_ptr<GlContext> context, map::MapAnnotationModel* model) :
    DrawItem(), p(std::make_unique<Impl>(std::move(context), model))
{
}
MapAnnotationsDrawItem::~MapAnnotationsDrawItem() = default;

void MapAnnotationsDrawItem::Impl::RebuildCommittedGeometry()
{
   modelStrokeVertices_.clear();
   modelFillVertices_.clear();
   pickSegments_.clear();
   pickCircles_.clear();
   const auto& geod = util::GeographicLib::DefaultGeodesic();

   auto handleObject = [&](const map::MapAnnotationObject& obj)
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
                  if (arg.points.size() >= 2)
                  {
                     const bool solidDash = (dash.first < 1.0e-3f);
                     if (!solidDash || !TryAppendSolidRoundPolylineGeosFill(
                                          modelFillVertices_,
                                          arg.points,
                                          obj.style.strokeWidthM,
                                          obj.style.strokeColor))
                     {
                        AppendPolylineStroke(modelStrokeVertices_,
                                             arg.points,
                                             false,
                                             obj.style.strokeWidthM,
                                             obj.style.strokeColor,
                                             geod,
                                             dash.first,
                                             dash.second);
                     }
                  }
                  else
                  {
                     const double radiusM =
                        obj.style.strokeWidthM.value() * 0.5;
                     AppendFilledGeoDisk(modelFillVertices_,
                                         arg.points.front(),
                                         radiusM,
                                         obj.style.strokeColor);
                  }
               }
               else
               {
                  AppendPolylineStroke(modelStrokeVertices_,
                                       arg.points,
                                       false,
                                       obj.style.strokeWidthM,
                                       obj.style.strokeColor,
                                       geod,
                                       dash.first,
                                       dash.second);
               }
               if (arg.points.size() >= 2)
               {
                  for (std::size_t i = 0; i + 1 < arg.points.size(); ++i)
                  {
                     pickSegments_.push_back(PickSegment {
                        .id          = obj.id,
                        .a           = arg.points[i],
                        .b           = arg.points[i + 1],
                        .halfStrokeM = obj.style.strokeWidthM * 0.5});
                  }
               }
               else if (arg.roundStroke && arg.points.size() == 1)
               {
                  pickCircles_.push_back(PickCircle {
                     .id          = obj.id,
                     .center      = arg.points[0],
                     .radiusM     = obj.style.strokeWidthM.value() * 0.5,
                     .halfStrokeM = obj.style.strokeWidthM * 0.5});
               }
            }
            else if constexpr (std::is_same_v<T, map::MapAnnotationCircle>)
            {
               const auto dash = DashAttribs(obj.style);
               if (obj.style.polygonFill)
               {
                  AppendFilledGeoDisk(modelFillVertices_,
                                      arg.center,
                                      arg.radiusMeters,
                                      obj.style.fillColor);
               }
               AppendCircleStroke(modelStrokeVertices_,
                                  arg.center,
                                  arg.radiusMeters,
                                  obj.style.strokeWidthM,
                                  obj.style.strokeColor,
                                  geod,
                                  dash.first,
                                  dash.second);
               pickCircles_.push_back(
                  PickCircle {.id          = obj.id,
                              .center      = arg.center,
                              .radiusM     = arg.radiusMeters,
                              .halfStrokeM = obj.style.strokeWidthM * 0.5});
            }
            else if constexpr (std::is_same_v<T, map::MapAnnotationRectangle>)
            {
               AppendRectangleStrokeAndFill(modelStrokeVertices_,
                                            modelFillVertices_,
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
               pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = ll,
                               .b           = lr,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = lr,
                               .b           = ur,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = ur,
                               .b           = ul,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
               pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = ul,
                               .b           = ll,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
            }
            else if constexpr (std::is_same_v<T, map::MapAnnotationMeasure>)
            {
               const double pinRadiusM = MeasurePinRadiusM(obj.style);
               const auto   dash       = DashAttribs(obj.style);
               const std::vector<common::Coordinate> seg = {arg.a, arg.b};
               AppendPolylineStroke(
                  modelStrokeVertices_,
                  seg,
                  false,
                  units::length::meters<double> {pinRadiusM * 0.8},
                  obj.style.strokeColor,
                  geod,
                  dash.first,
                  dash.second);
               AppendFilledGeoDisk(
                  modelFillVertices_, arg.a, pinRadiusM, obj.style.strokeColor);
               AppendFilledGeoDisk(
                  modelFillVertices_, arg.b, pinRadiusM, obj.style.strokeColor);
               AppendFilledGeoDisk(modelFillVertices_,
                                   arg.a,
                                   pinRadiusM * 0.38,
                                   {1.0f, 1.0f, 1.0f, 0.95f});
               AppendFilledGeoDisk(modelFillVertices_,
                                   arg.b,
                                   pinRadiusM * 0.38,
                                   {1.0f, 1.0f, 1.0f, 0.95f});
               pickCircles_.push_back(PickCircle {
                  .id          = obj.id,
                  .center      = arg.a,
                  .radiusM     = pinRadiusM,
                  .halfStrokeM = units::length::meters<double> {0.0}});
               pickCircles_.push_back(PickCircle {
                  .id          = obj.id,
                  .center      = arg.b,
                  .radiusM     = pinRadiusM,
                  .halfStrokeM = units::length::meters<double> {0.0}});
               pickSegments_.push_back(
                  PickSegment {.id          = obj.id,
                               .a           = arg.a,
                               .b           = arg.b,
                               .halfStrokeM = obj.style.strokeWidthM * 0.5});
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
               handleObject(o);
            }
         });
   }

   strokeModelCount_ =
      static_cast<GLsizei>(modelStrokeVertices_.size() / kFloatsPerVertex);
   fillModelCount_ =
      static_cast<GLsizei>(modelFillVertices_.size() / kFloatsPerVertex);
   gpuModelDirty_ = true;
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
      return;
   }
   if (!previewRoundStroke_ && previewPts_.size() < 2)
   {
      gpuPreviewDirty_ = true;
      return;
   }
   if (previewRoundStroke_ && previewPts_.empty())
   {
      gpuPreviewDirty_ = true;
      return;
   }

   const auto& geod = util::GeographicLib::DefaultGeodesic();
   if (previewRoundStroke_)
   {
      if (previewPts_.size() >= 2)
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
      else
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

   strokePreviewCount_ =
      static_cast<GLsizei>(previewStrokeVertices_.size() / kFloatsPerVertex);
   fillPreviewCount_ =
      static_cast<GLsizei>(previewFillVertices_.size() / kFloatsPerVertex);
   gpuPreviewDirty_ = true;
}

void MapAnnotationsDrawItem::Initialize()
{
   if (p->vaoModelStroke_ != 0)
   {
      Deinitialize();
   }

   p->shader_ = p->context_->GetShaderProgram(":/gl/annotation_geo.vert",
                                              ":/gl/annotation_stroke.frag");

   p->uMapMatrixLoc_ = p->shader_->GetUniformLocation("uMapMatrix");
   p->uOriginLoc_    = p->shader_->GetUniformLocation("uOriginLatLong");
   p->uHatchModeLoc_ = p->shader_->GetUniformLocation("uHatchMode");

   glGenVertexArrays(1, &p->vaoModelStroke_);
   glGenBuffers(1, &p->vboModelStroke_);
   ConfigureAnnotationVaoForVbo(p->vaoModelStroke_, p->vboModelStroke_);

   glGenVertexArrays(1, &p->vaoPreviewStroke_);
   glGenBuffers(1, &p->vboPreviewStroke_);
   ConfigureAnnotationVaoForVbo(p->vaoPreviewStroke_, p->vboPreviewStroke_);

   glGenVertexArrays(1, &p->vaoModelFill_);
   glGenBuffers(1, &p->vboModelFill_);
   ConfigureAnnotationVaoForVbo(p->vaoModelFill_, p->vboModelFill_);

   glGenVertexArrays(1, &p->vaoPreviewFill_);
   glGenBuffers(1, &p->vboPreviewFill_);
   ConfigureAnnotationVaoForVbo(p->vaoPreviewFill_, p->vboPreviewFill_);

   Rebuild();
}

void MapAnnotationsDrawItem::Deinitialize()
{
   glDeleteBuffers(1, &p->vboModelStroke_);
   glDeleteVertexArrays(1, &p->vaoModelStroke_);
   glDeleteBuffers(1, &p->vboPreviewStroke_);
   glDeleteVertexArrays(1, &p->vaoPreviewStroke_);
   glDeleteBuffers(1, &p->vboModelFill_);
   glDeleteVertexArrays(1, &p->vaoModelFill_);
   glDeleteBuffers(1, &p->vboPreviewFill_);
   glDeleteVertexArrays(1, &p->vaoPreviewFill_);
   p->vboModelStroke_   = 0;
   p->vaoModelStroke_   = 0;
   p->vboPreviewStroke_ = 0;
   p->vaoPreviewStroke_ = 0;
   p->vboModelFill_     = 0;
   p->vaoModelFill_     = 0;
   p->vboPreviewFill_   = 0;
   p->vaoPreviewFill_   = 0;
}

void MapAnnotationsDrawItem::SetPreviewPolyline(
   const std::vector<common::Coordinate>& pts,
   const map::MapAnnotationStyle&         style,
   bool                                   roundStroke)
{
   p->previewPts_         = pts;
   p->previewStyle_       = style;
   p->previewRoundStroke_ = roundStroke;
   p->previewActive_      = true;
   p->RebuildPreviewGeometry();
}

void MapAnnotationsDrawItem::ClearPreview()
{
   p->previewActive_      = false;
   p->previewRoundStroke_ = false;
   p->previewPts_.clear();
   p->previewStrokeVertices_.clear();
   p->previewFillVertices_.clear();
   p->strokePreviewCount_ = 0;
   p->fillPreviewCount_   = 0;
   p->gpuPreviewDirty_    = true;
}

void MapAnnotationsDrawItem::Rebuild()
{
   p->RebuildCommittedGeometry();
}

static float
PointSegDist2Screen(const glm::vec2& m, const glm::vec2& a, const glm::vec2& b)
{
   const glm::vec2 ab    = b - a;
   const float     denom = glm::dot(ab, ab);
   if (denom < 1e-30f)
   {
      return glm::dot(m - a, m - a);
   }
   float t                 = glm::dot(m - a, ab) / denom;
   t                       = std::clamp(t, 0.0f, 1.0f);
   const glm::vec2 closest = a + t * ab;
   return glm::dot(m - closest, m - closest);
}

std::vector<std::uint64_t>
MapAnnotationsDrawItem::PickObjects(const glm::vec2&          mouseMapCoords,
                                    const common::Coordinate& mouseGeo) const
{
   const glm::vec2 m = mouseMapCoords;

   constexpr float                              kPickScale2 = 1.0e-5f;
   std::vector<std::pair<float, std::uint64_t>> hits;
   hits.reserve(p->pickSegments_.size() + p->pickCircles_.size());

   for (const auto& seg : p->pickSegments_)
   {
      const glm::vec2 sa = util::maplibre::LatLongToScreenCoordinate(
         {seg.a.latitude_, seg.a.longitude_});
      const glm::vec2 sb = util::maplibre::LatLongToScreenCoordinate(
         {seg.b.latitude_, seg.b.longitude_});
      const float d2 = PointSegDist2Screen(m, sa, sb);
      if (d2 < kPickScale2)
      {
         hits.emplace_back(d2, seg.id);
      }
   }

   for (const auto& c : p->pickCircles_)
   {
      const double distM = util::GeographicLib::GetDistance(c.center.latitude_,
                                                            c.center.longitude_,
                                                            mouseGeo.latitude_,
                                                            mouseGeo.longitude_)
                              .value();
      const double ringDist =
         std::abs(distM - c.radiusM) - c.halfStrokeM.value();
      constexpr double kTolM = 1500.0;
      if (ringDist < kTolM)
      {
         hits.emplace_back(0.0f, c.id);
      }
   }

   std::sort(hits.begin(),
             hits.end(),
             [](const auto& lhs, const auto& rhs)
             {
                if (lhs.first != rhs.first)
                {
                   return lhs.first < rhs.first;
                }
                return lhs.second < rhs.second;
             });

   std::vector<std::uint64_t> ids;
   ids.reserve(hits.size());
   for (const auto& [distance2, id] : hits)
   {
      static_cast<void>(distance2);
      if (std::find(ids.begin(), ids.end(), id) == ids.end())
      {
         ids.push_back(id);
      }
   }

   return ids;
}

void MapAnnotationsDrawItem::Render(
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (p->gpuModelDirty_)
   {
      glBindBuffer(GL_ARRAY_BUFFER, p->vboModelStroke_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(sizeof(float) *
                                           p->modelStrokeVertices_.size()),
                   p->modelStrokeVertices_.data(),
                   GL_DYNAMIC_DRAW);

      glBindBuffer(GL_ARRAY_BUFFER, p->vboModelFill_);
      glBufferData(
         GL_ARRAY_BUFFER,
         static_cast<GLsizeiptr>(sizeof(float) * p->modelFillVertices_.size()),
         p->modelFillVertices_.data(),
         GL_DYNAMIC_DRAW);
      p->gpuModelDirty_ = false;
   }

   if (p->gpuPreviewDirty_)
   {
      glBindBuffer(GL_ARRAY_BUFFER, p->vboPreviewStroke_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(sizeof(float) *
                                           p->previewStrokeVertices_.size()),
                   p->previewStrokeVertices_.data(),
                   GL_DYNAMIC_DRAW);

      glBindBuffer(GL_ARRAY_BUFFER, p->vboPreviewFill_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(sizeof(float) *
                                           p->previewFillVertices_.size()),
                   p->previewFillVertices_.data(),
                   GL_DYNAMIC_DRAW);
      p->gpuPreviewDirty_ = false;
   }

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   p->shader_->Use();
   UseMapProjection(params, p->uMapMatrixLoc_, p->uOriginLoc_);

   if (p->fillModelCount_ > 0)
   {
      glUniform1i(p->uHatchModeLoc_, 0);
      glBindVertexArray(p->vaoModelFill_);
      glDrawArrays(GL_TRIANGLES, 0, p->fillModelCount_);
   }

   if (p->fillPreviewCount_ > 0)
   {
      glUniform1i(p->uHatchModeLoc_, 0);
      glBindVertexArray(p->vaoPreviewFill_);
      glDrawArrays(GL_TRIANGLES, 0, p->fillPreviewCount_);
   }

   if (p->strokeModelCount_ > 0)
   {
      glUniform1i(p->uHatchModeLoc_, 0);
      glBindVertexArray(p->vaoModelStroke_);
      glDrawArrays(GL_TRIANGLES, 0, p->strokeModelCount_);
   }

   if (p->strokePreviewCount_ > 0)
   {
      glUniform1i(p->uHatchModeLoc_, 0);
      glBindVertexArray(p->vaoPreviewStroke_);
      glDrawArrays(GL_TRIANGLES, 0, p->strokePreviewCount_);
   }

   glBindVertexArray(0);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,modernize-use-auto,cppcoreguidelines-pro-type-cstyle-cast,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays,cppcoreguidelines-pro-bounds-pointer-arithmetic,performance-no-int-to-ptr)
} // namespace scwx::qt::gl::draw
