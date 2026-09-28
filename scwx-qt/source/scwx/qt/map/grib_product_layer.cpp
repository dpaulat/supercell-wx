#include <scwx/qt/map/grib_product_layer.hpp>
#include <scwx/qt/gl/shader_program.hpp>
#include <scwx/qt/manager/grib_manager.hpp>
#include <scwx/qt/map/grib_frame_info.hpp>
#include <scwx/qt/settings/palette_settings.hpp>
#include <scwx/qt/util/file.hpp>
#include <scwx/qt/util/tooltip.hpp>
#include <scwx/common/color_table.hpp>
#include <scwx/util/logger.hpp>

#include <fmt/format.h>

#if defined(_MSC_VER)
#   pragma warning(push, 0)
#endif

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <mbgl/util/constants.hpp>

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <QGuiApplication>
#include <QTimer>

namespace scwx::qt::map
{

static const std::string logPrefix_ = "scwx::qt::map::grib_product_layer";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

// Default/initial values only, used until the first frame loads -- once a
// frame is parsed, its own colorOffset/colorScale/noDataThreshold (set by
// GribManager per product, see grib-helper's decode_grib) take over. These
// match the real base-reflectivity palette's defined range (see
// res/palettes/wct/DR.pal: -20 to 75 dBZ), the same product this layer
// showed before per-product ranges existed.
static constexpr float kDefaultDataMomentOffset_ = -20.0f;
static constexpr float kDefaultDataMomentScale_  = 95.0f;
static constexpr float kDefaultNoDataThreshold_  = 0.0f;

// Same palette key NEXRAD base reflectivity uses (PaletteSettings::palette
// ("BR")) -- reused for every MRMS product for now, not just reflectivity,
// since Supercell has no user-configurable palette for rotation
// track/hail/precip. Visually mismatched hue-wise for those, but shows
// relative intensity correctly; a real per-product palette is future work.
static const std::string kPaletteKey_ = "BR";

namespace
{

// GRIB2's Grid Definition Section is self-describing (see decode_grib's
// GridType detection off eccodes' "gridType" key) -- MRMS is regular_ll
// (uniform degree spacing), RTMA and most other NCEP CONUS-nest models
// (HRRR, RRFS, RAP, NAM, for future sources) are lambert. The two need
// genuinely different mesh math, not just different parameters: on a
// regular_ll grid, a column of constant grid index is a line of constant
// longitude and a row is a line of constant latitude, so the existing
// mesh only needed to subdivide latitude (see the west/east comment in
// LoadFrame). On a lambert grid neither is true -- rows and columns of
// constant grid index are curves in lat/lon space -- so both axes need
// subdividing, with each vertex's true geographic position computed via
// the actual projection.
enum class GridType
{
   RegularLatLon,
   Lambert
};

// LambertGrid/LambertConstants/ComputeLambertConstants/LambertForward/
// LambertInverse/LambertGridToLatLon now live in grib_frame_info.hpp --
// promoted there once WindBarbLayer became a second consumer of the same
// Snyder projection math (see that header for the full derivation notes).

} // namespace

class GribProductLayer::Impl
{
public:
   explicit Impl(GribCategory category) : category_ {category} {}
   ~Impl() = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;

   GribCategory category_;

   std::shared_ptr<gl::ShaderProgram> shaderProgram_ {nullptr};

   GLint uMVPMatrixLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uOriginLatLongLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uDataMomentOffsetLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uDataMomentScaleLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uNoDataThresholdLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uDataTextureLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uPaletteLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};
   GLint uContourIntervalLocation_ {static_cast<GLint>(GL_INVALID_INDEX)};

   GLuint  vao_ {GL_INVALID_INDEX};
   GLuint  vbo_ {GL_INVALID_INDEX};
   GLuint  dataTexture_ {GL_INVALID_INDEX};
   GLuint  paletteTexture_ {GL_INVALID_INDEX};
   GLsizei numVertices_ {0};

   bool frameLoaded_ {false};

   // Set by the reload timer (cheap stat() only), consumed by Render() --
   // GL calls only happen there, never from the timer callback directly,
   // since MapLibre only guarantees this layer's GL context is current
   // inside its own Initialize()/Render() calls. Mirrors how
   // RadarProductLayer's sweepNeedsUpdate_ works (set from a signal
   // handler, applied lazily in Render()).
   bool frameNeedsReload_ {false};

   // Same deferred-to-Render() pattern as frameNeedsReload_, set when the
   // user changes their reflectivity palette in Settings.
   bool paletteNeedsRebuild_ {false};

   // Grid geometry, populated by LoadFrame()
   GridType gridType_ {GridType::RegularLatLon};
   long     nx_ {};
   long     ny_ {};
   double   lat1_ {};
   double   lon1_ {};
   double   di_ {};
   double   dj_ {};

   // Lambert only (see GridType/LambertGrid comments above); left
   // zero-initialized and unused for regular_ll frames.
   double lov_ {};
   double lad_ {};
   double latin1_ {};
   double latin2_ {};
   double dx_ {};
   double dy_ {};
   double radius_ {};

   // Raw decoded grid, row-major (index = row * nx_ + col, i.e. column
   // fastest), kept around after the GL upload specifically so
   // RunMousePicking can look up the value under the cursor for the
   // Shift-hover data tooltip -- see ValueAt().
   std::vector<float> values_;

   // Derived from the MRMS filename by grib-helper, since eccodes has no
   // usable product name for MRMS's local GRIB2 table (see
   // grib-helper/README.md). Kept as two separate fields, not one display
   // string -- validTime_ (ISO8601 UTC) is meant to be real, usable data
   // (e.g. a future staleness check against TimelineManager's selected
   // time), not just text baked into a label.
   std::string productLabel_; // e.g. "MergedReflectivityQCComposite_00.50"
   std::string validTime_;    // e.g. "2026-09-19T03:58:38Z"

   // Per-frame colorizing range -- set by GribManager per product (see
   // grib_manager.cpp's ProductConfig), read back from the frame header
   // rather than assumed, so switching products at runtime recolors
   // correctly instead of staying stuck on reflectivity's range.
   float colorOffset_     = kDefaultDataMomentOffset_;
   float colorScale_      = kDefaultDataMomentScale_;
   float noDataThreshold_ = kDefaultNoDataThreshold_;

   // 0 (default) is fill mode; a nonzero value switches the shader to
   // isoline rendering at every multiple of this value (see grib.frag)
   // -- set by GribManager per product (ProductConfig::contourInterval),
   // baked into the frame header by decode_grib the same way colorOffset/
   // colorScale/noDataThreshold already are. Also drives dataTexture_'s
   // filter mode in LoadFrame() -- see that comment for why.
   float contourInterval_ = 0.0f;

   // Keeps GribManager alive: Instance() only caches a weak_ptr, so
   // discarding the shared_ptr immediately destroys it (and its timer)
   // before it ever gets to poll.
   std::shared_ptr<manager::GribManager> gribManager_;

   // Fallback safety net for noticing a new frame: GribManager::FrameReady
   // (see frameReadyConnection_ below) is the primary, immediate path now
   // -- this cheap stat()-only poll only matters if a signal were ever
   // somehow missed (e.g. a frame written by some other means entirely).
   QTimer*                         reloadTimer_ {nullptr};
   std::filesystem::file_time_type lastFrameWriteTime_ {};

   boost::signals2::scoped_connection paletteChangedConnection_;

   // Reassigned each Initialize() (see the disconnect-then-connect there,
   // guarding against a second Initialize() without an intervening
   // Deinitialize() leaving two live connections, same reasoning as
   // reloadTimer_'s delete-before-new guard).
   QMetaObject::Connection frameReadyConnection_;
};

GribProductLayer::GribProductLayer(std::shared_ptr<gl::GlContext> glContext,
                                   GribCategory                   category) :
    GenericLayer(std::move(glContext)), p(std::make_unique<Impl>(category))
{
}
GribProductLayer::~GribProductLayer() = default;

void GribProductLayer::Initialize(
   const std::shared_ptr<MapContext>& /* mapContext */)
{
   logger_->debug("Initialize()");

   // Ensures the poller is constructed/running (singleton, lazy-init on
   // first Instance() call -- see manager::GribManager). Its FrameReady
   // signal (connected below) is what actually tells this layer to
   // reload.
   p->gribManager_ = manager::GribManager::Instance(p->category_);

   // Drain any GL error state inherited from elsewhere (observed: MapLibre's
   // own internal rendering leaves a stray GL_INVALID_VALUE (1281) that has
   // nothing to do with this layer -- confirmed by checking here, before any
   // of this layer's own GL calls run). Without this, that error gets
   // misattributed to whichever layer happens to check glGetError() first.
   SCWX_GL_CHECK_ERROR();

   auto glContext = gl_context();

   p->shaderProgram_ =
      glContext->GetShaderProgram(":/gl/grib.vert", ":/gl/grib.frag");

   p->uMVPMatrixLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uMVPMatrix");
   p->uOriginLatLongLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uOriginLatLong");
   p->uDataMomentOffsetLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uDataMomentOffset");
   p->uDataMomentScaleLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uDataMomentScale");
   p->uNoDataThresholdLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uNoDataThreshold");
   p->uDataTextureLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uDataTexture");
   p->uPaletteLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uPalette");
   p->uContourIntervalLocation_ =
      glGetUniformLocation(p->shaderProgram_->id(), "uContourInterval");

   // Sampler-to-texture-unit bindings don't change per frame; set them once
   // here rather than every Render(). Without this, both samplers default
   // to unit 0 and the palette would sample the raw data texture.
   p->shaderProgram_->Use();
   glUniform1i(p->uDataTextureLocation_, 0);
   glUniform1i(p->uPaletteLocation_, 1);

   glGenVertexArrays(1, &p->vao_);
   glGenBuffers(1, &p->vbo_);
   glGenTextures(1, &p->dataTexture_);
   glGenTextures(1, &p->paletteTexture_);

   BuildPalette();
   LoadFrame();

   // Live-update when the user changes their reflectivity palette in
   // Settings (mirrors how map_widget.cpp subscribes for RadarProductLayer).
   // Deferred to Render() for the same reason as the reload timer below --
   // this signal can fire at an arbitrary time, not necessarily with this
   // layer's GL context current.
   p->paletteChangedConnection_ = settings::PaletteSettings::Instance()
                                     .palette(kPaletteKey_)
                                     .changed_signal()
                                     .connect(
                                        [this](auto&&...)
                                        {
                                           p->paletteNeedsRebuild_ = true;
                                           Q_EMIT NeedsRendering();
                                        });

   // Primary reload path: GribManager may finish a fetch on its own
   // background thread (see grib_manager.cpp's fetchPool_), so this
   // connection can fire from a thread other than this one -- Qt's queued
   // cross-thread delivery makes that safe, and the slot only ever sets a
   // dirty flag (never touches GL directly), same pattern as
   // paletteChangedConnection_ above.
   QObject::disconnect(p->frameReadyConnection_); // guard against a second
                                                  // Initialize() leaving
                                                  // two live connections
   p->frameReadyConnection_ =
      connect(p->gribManager_.get(),
              &manager::GribManager::FrameReady,
              this,
              [this](std::size_t productIndex)
              {
                 // GribManager can now have several products active at once
                 // (see its own SetProductActive), but this layer still only
                 // ever renders one frame -- the current/primary one -- so a
                 // FrameReady for any other active product isn't relevant here
                 // yet. Rendering every active product at once is a real,
                 // separate follow-up, not done in this pass.
                 if (productIndex == p->gribManager_->CurrentProductIndex())
                 {
                    p->frameNeedsReload_ = true;
                    Q_EMIT NeedsRendering();
                 }
              });

   // Fallback safety net only -- see reloadTimer_'s comment. Kept slow
   // since FrameReady is now the responsive path.
   constexpr int kReloadCheckIntervalMs = 15000;
   delete p->reloadTimer_; // guard against a second Initialize() without an
                           // intervening Deinitialize() leaking a timer
   p->reloadTimer_ = new QTimer(this);
   connect(p->reloadTimer_,
           &QTimer::timeout,
           this,
           [this]()
           {
              const std::string framePath = GetGribFramePath(
                 p->category_, p->gribManager_->CurrentProductIndex());
              try
              {
                 if (std::filesystem::last_write_time(framePath) !=
                     p->lastFrameWriteTime_)
                 {
                    p->frameNeedsReload_ = true;
                    Q_EMIT NeedsRendering();
                 }
              }
              catch (const std::filesystem::filesystem_error&)
              {
                 // Frame file missing/unreadable -- nothing to reload yet.
              }
           });
   p->reloadTimer_->start(kReloadCheckIntervalMs);

   SCWX_GL_CHECK_ERROR();
}

void GribProductLayer::BuildPalette()
{
   // Same palette the user has selected for NEXRAD base reflectivity
   // (Settings > Palettes > Color Tables), not a hardcoded ramp -- see
   // PaletteSettings::palette("BR"). Fallback chain mirrors
   // MapWidgetImpl::UpdateColorTable exactly (map_widget.cpp): configured
   // file, then the key's built-in default, and ColorTable::IsValid() is
   // checked the same way.
   auto& paletteSetting =
      settings::PaletteSettings::Instance().palette(kPaletteKey_);

   std::string colorTableFile = paletteSetting.GetValue();
   if (colorTableFile.empty())
   {
      colorTableFile = paletteSetting.GetDefault();
   }

   std::unique_ptr<std::istream> colorTableStream =
      util::OpenFile(colorTableFile);
   if (colorTableStream->fail())
   {
      logger_->warn("Could not open color table {}", colorTableFile);
      colorTableStream = util::OpenFile(paletteSetting.GetDefault());
   }

   std::shared_ptr<common::ColorTable> colorTable =
      common::ColorTable::Load(*colorTableStream);
   if (!colorTable->IsValid())
   {
      logger_->warn("Could not load color table {}", colorTableFile);
      colorTableStream = util::OpenFile(paletteSetting.GetDefault());
      colorTable       = common::ColorTable::Load(*colorTableStream);
   }

   // Deliberately NOT p->colorOffset_/colorScale_ here -- those are the
   // *product's* physical range (e.g. 260-325 Kelvin for RTMA temperature),
   // used by the shader to normalize a raw data value into a 0..1 LUT
   // index (see uDataMomentOffset/Scale in Render()). This loop is a
   // completely different concern: sampling the *color table's own*
   // native gradient to build the 256-entry LUT texture in the first
   // place, which must always use the table's own domain -- DR.pal's
   // breakpoints are dBZ values roughly -20 to 75, regardless of what
   // product is being displayed. Confirmed as a real, live bug: feeding
   // RTMA's Kelvin range in here instead put every LUT entry past DR.pal's
   // highest breakpoint, so ColorTable::Color() clamped all 256 entries to
   // the same final color -- a solid, uninformative fill. Reflectivity
   // never showed this because its own physical range (-20 to 75 dBZ)
   // happens to equal kDefaultDataMomentOffset_/Scale_ already, by
   // coincidence, not by correct design.
   std::array<boost::gil::rgba8_pixel_t, 256> palette {};
   for (int i = 0; i < 256; ++i)
   {
      const float t = static_cast<float>(i) / 255.0f;
      const float value =
         kDefaultDataMomentOffset_ + t * kDefaultDataMomentScale_;
      palette[i] = colorTable->Color(value);
   }

   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_1D, p->paletteTexture_);
   glTexImage1D(GL_TEXTURE_1D,
                0,
                GL_RGBA,
                static_cast<GLsizei>(palette.size()),
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                palette.data());
   glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

void GribProductLayer::LoadFrame()
{
   const std::string framePath =
      GetGribFramePath(p->category_, p->gribManager_->CurrentProductIndex());

   std::ifstream in(framePath, std::ios::binary);
   if (!in)
   {
      logger_->warn("Could not open frame file: {}", framePath);
      return;
   }

   try
   {
      p->lastFrameWriteTime_ = std::filesystem::last_write_time(framePath);
   }
   catch (const std::filesystem::filesystem_error&)
   {
      // Non-fatal: worst case, the next reload-timer tick re-reads a frame
      // that hasn't actually changed.
   }

   std::string header;
   std::getline(in, header);

   try
   {
      p->nx_   = static_cast<long>(ExtractNumber(header, "nx"));
      p->ny_   = static_cast<long>(ExtractNumber(header, "ny"));
      p->lat1_ = ExtractNumber(header, "lat1");
      p->lon1_ = ExtractNumber(header, "lon1");
      p->di_   = ExtractNumber(header, "di");
      p->dj_   = ExtractNumber(header, "dj");
      p->gridType_ =
         ExtractStringOr(header, "gridType", "regular_ll") == "lambert" ?
            GridType::Lambert :
            GridType::RegularLatLon;
      p->lov_    = ExtractNumberOr(header, "lov", 0.0);
      p->lad_    = ExtractNumberOr(header, "lad", 0.0);
      p->latin1_ = ExtractNumberOr(header, "latin1", 0.0);
      p->latin2_ = ExtractNumberOr(header, "latin2", 0.0);
      p->dx_     = ExtractNumberOr(header, "dx", 0.0);
      p->dy_     = ExtractNumberOr(header, "dy", 0.0);
      p->radius_ = ExtractNumberOr(header, "radius", 0.0);
      // Prefer GribManager's curated display name over the header's own
      // "product" field: decode_grib's label is a clean product+level
      // string for MRMS, but just the bare GRIB shortName for RTMA (e.g.
      // "2t", since field selection there happens by shortName) --
      // GribManager already knows the pretty name shown in the dropdown.
      p->productLabel_ = p->gribManager_ ?
                            p->gribManager_->CurrentProductName() :
                            ExtractString(header, "product");
      p->validTime_    = ExtractString(header, "validTime");
      p->colorOffset_  = static_cast<float>(
         ExtractNumberOr(header, "colorOffset", kDefaultDataMomentOffset_));
      p->colorScale_ = static_cast<float>(
         ExtractNumberOr(header, "colorScale", kDefaultDataMomentScale_));
      p->noDataThreshold_ = static_cast<float>(
         ExtractNumberOr(header, "noDataThreshold", kDefaultNoDataThreshold_));
      p->contourInterval_ =
         static_cast<float>(ExtractNumberOr(header, "contourInterval", 0.0));
      const auto byteLength =
         static_cast<size_t>(ExtractNumber(header, "byteLength"));

      // Kept on Impl (not a local, discarded after the GL upload) so
      // RunMousePicking can look up the actual value under the cursor for
      // the Shift-hover data tooltip -- mirrors RadarProductView keeping
      // its own decoded moments around for GetDataValue().
      p->values_.resize(byteLength / sizeof(float));
      in.read(reinterpret_cast<char*>(p->values_.data()),
              static_cast<std::streamsize>(byteLength));

      if (!in || p->values_.size() != static_cast<size_t>(p->nx_ * p->ny_))
      {
         logger_->warn("Frame payload size mismatch, expected {} got {}",
                       p->nx_ * p->ny_,
                       p->values_.size());
         return;
      }

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, p->dataTexture_);
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_R32F,
                   static_cast<GLsizei>(p->nx_),
                   static_cast<GLsizei>(p->ny_),
                   0,
                   GL_RED,
                   GL_FLOAT,
                   p->values_.data());

      // NEAREST for every fill-mode product -- load-bearing, not just a
      // style choice: MRMS's -999 "no coverage" sentinel (and any other
      // product's own noDataThreshold cutoff) must never blend into a
      // real neighboring value at a texel boundary. Contour mode is the
      // one exception: grib.frag's isoline math needs dFdx/dFdy of the
      // sampled value to reflect the real data gradient, and NEAREST
      // filtering makes that spike at every texel edge instead (the
      // sampled value is a step function, not a smooth one) -- LINEAR
      // is safe here specifically because no contour-mode product uses a
      // sentinel value the way MRMS does.
      const GLint filter =
         (p->contourInterval_ > 0.0f) ? GL_LINEAR : GL_NEAREST;
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      std::vector<float> vertices;

      auto pushVertex =
         [&vertices](double lat, double lon, float texU, float texV)
      {
         vertices.push_back(static_cast<float>(lat));
         vertices.push_back(static_cast<float>(lon));
         vertices.push_back(texU);
         vertices.push_back(texV);
      };

      if (p->gridType_ == GridType::RegularLatLon)
      {
         // Build the bounding quad. lat1/lon1 is the north-west corner;
         // grid scans east (+di) and south (-dj), matching MRMS/most
         // GRIB2 default scanning order (see grib-helper/README.md).
         const double west  = p->lon1_;
         const double east  = p->lon1_ + p->di_ * static_cast<double>(p->nx_);
         const double north = p->lat1_;
         const double south = p->lat1_ - p->dj_ * static_cast<double>(p->ny_);

         // A single quad isn't enough: GL interpolates texCoord linearly
         // in screen space, but screen space here is Web Mercator, which
         // is nonlinear in latitude (increasingly stretched toward the
         // poles), while the texture data is a plain linear lat/lon grid.
         // Across a quad spanning CONUS's full ~35 degrees of latitude
         // that mismatch is large -- confirmed empirically at ~2 degrees
         // around 42N, right where Mercator's curvature deviates most
         // from a straight line between the north/south corners.
         // RadarProductLayer never hits this because each of its
         // triangles covers a tiny sliver of the sweep, far too small for
         // the curvature to matter.
         //
         // Fix: subdivide along latitude only (longitude doesn't need it
         // -- Mercator-X is linear in longitude) into enough thin strips
         // that linear interpolation is accurate within each one.
         // Longitude stays 2 columns (west, east); this is a vertical
         // resolution knob, not a full per-data-point mesh like radar's.
         constexpr int kLatSubdivisions = 128;

         vertices.reserve(static_cast<size_t>(kLatSubdivisions + 1) * 2 * 4);

         for (int i = 0; i <= kLatSubdivisions; ++i)
         {
            const double f    = static_cast<double>(i) / kLatSubdivisions;
            const double lat  = north - f * (north - south);
            const auto   texV = static_cast<float>(f);

            // West then east column, in this order, so GL_TRIANGLE_STRIP
            // connects consecutive rows into the correct quads.
            pushVertex(lat, west, 0.0f, texV);
            pushVertex(lat, east, 1.0f, texV);
         }

         p->numVertices_ = static_cast<GLsizei>((kLatSubdivisions + 1) * 2);
      }
      else // GridType::Lambert
      {
         // Unlike regular_ll, a row or column of constant grid index here
         // is a curve in lat/lon space, not a straight line -- see
         // GridType's comment above. So both axes need subdividing, each
         // vertex's true position computed via LambertGridToLatLon rather
         // than linear degree interpolation. Resolution chosen to match
         // regular_ll's kLatSubdivisions in order of magnitude; mesh is
         // rebuilt only on frame load (RTMA is hourly), so its cost is a
         // non-issue.
         constexpr int kMeshCols = 96;
         constexpr int kMeshRows = 96;

         const LambertGrid lambertGrid {p->lov_,
                                        p->lad_,
                                        p->latin1_,
                                        p->latin2_,
                                        p->lat1_,
                                        p->lon1_,
                                        p->dx_,
                                        p->dy_,
                                        p->radius_};

         vertices.reserve(
            static_cast<size_t>((kMeshRows + 1) * (kMeshCols + 1) +
                                kMeshRows * 2) *
            4);

         auto vertexAt = [&](int row, int col) -> glm::dvec2
         {
            const double iFrac = static_cast<double>(col) / kMeshCols;
            const double jFrac = static_cast<double>(row) / kMeshRows;
            const double i     = iFrac * static_cast<double>(p->nx_ - 1);
            const double j     = jFrac * static_cast<double>(p->ny_ - 1);
            return LambertGridToLatLon(lambertGrid, i, j);
         };

         for (int row = 0; row < kMeshRows; ++row)
         {
            const auto texV0 = static_cast<float>(row) / kMeshRows;
            const auto texV1 = static_cast<float>(row + 1) / kMeshRows;

            for (int col = 0; col <= kMeshCols; ++col)
            {
               const auto texU    = static_cast<float>(col) / kMeshCols;
               glm::dvec2 latLon0 = vertexAt(row, col);
               glm::dvec2 latLon1 = vertexAt(row + 1, col);

               pushVertex(latLon0.x, latLon0.y, texU, texV0);
               pushVertex(latLon1.x, latLon1.y, texU, texV1);
            }

            // Degenerate triangles bridging to the next row-band (repeat
            // this band's last vertex, then the next band's first
            // vertex) so the whole mesh still draws with a single
            // GL_TRIANGLE_STRIP call, matching Render()'s existing
            // glDrawArrays -- same technique the regular_ll path gets for
            // free by only ever having one row-band.
            if (row + 1 < kMeshRows)
            {
               const glm::dvec2 lastOfBand = vertexAt(row + 1, kMeshCols);
               pushVertex(lastOfBand.x, lastOfBand.y, 1.0f, texV1);

               const glm::dvec2 firstOfNext = vertexAt(row + 1, 0);
               pushVertex(firstOfNext.x, firstOfNext.y, 0.0f, texV1);
            }
         }

         p->numVertices_ = static_cast<GLsizei>(vertices.size() / 4);
      }

      glBindVertexArray(p->vao_);
      glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                   vertices.data(),
                   GL_STATIC_DRAW);

      constexpr GLsizei stride = 4 * sizeof(float);
      glVertexAttribPointer(
         0, 2, GL_FLOAT, GL_FALSE, stride, static_cast<void*>(0));
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1,
                            2,
                            GL_FLOAT,
                            GL_FALSE,
                            stride,
                            reinterpret_cast<void*>(2 * sizeof(float)));
      glEnableVertexAttribArray(1);

      // Rebuild the palette LUT using this frame's (possibly new)
      // colorOffset_/colorScale_ -- the LUT bakes in a specific value
      // range at build time, so switching products without rebuilding it
      // would leave the old product's range in effect.
      BuildPalette();

      p->frameLoaded_ = true;

      logger_->info(
         "Loaded GRIB frame: {} valid {} ({} x {} grid, origin ({}, {}))",
         p->productLabel_,
         p->validTime_,
         p->nx_,
         p->ny_,
         p->lat1_,
         p->lon1_);
   }
   catch (const std::exception& e)
   {
      logger_->warn("Failed to parse frame header: {}", e.what());
   }
}

void GribProductLayer::Render(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const QMapLibre::CustomLayerRenderParameters& params)
{
   if (p->paletteNeedsRebuild_)
   {
      p->paletteNeedsRebuild_ = false;
      BuildPalette();
   }

   if (p->frameNeedsReload_)
   {
      p->frameNeedsReload_ = false;
      LoadFrame();
   }

   if (!p->frameLoaded_)
   {
      return;
   }

   p->shaderProgram_->Use();

   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   const double scale  = std::pow(2.0, params.zoom) * 2.0 *
                         mbgl::util::tileSize_D / mbgl::util::DEGREES_MAX;
   const auto   xScale = static_cast<float>(scale / params.width);
   const auto   yScale = static_cast<float>(scale / params.height);

   glm::mat4 uMVPMatrix(1.0f);
   uMVPMatrix = glm::scale(uMVPMatrix, glm::vec3(xScale, yScale, 1.0f));
   uMVPMatrix = glm::rotate(uMVPMatrix,
                            glm::radians(static_cast<float>(params.bearing)),
                            glm::vec3(0.0f, 0.0f, 1.0f));

   glUniform2fv(p->uOriginLatLongLocation_,
                1,
                glm::value_ptr(glm::vec2 {params.latitude, params.longitude}));
   glUniformMatrix4fv(
      p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(uMVPMatrix));

   glUniform1f(p->uDataMomentOffsetLocation_, p->colorOffset_);
   glUniform1f(p->uDataMomentScaleLocation_, p->colorScale_);
   glUniform1f(p->uNoDataThresholdLocation_, p->noDataThreshold_);
   glUniform1f(p->uContourIntervalLocation_, p->contourInterval_);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, p->dataTexture_);
   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_1D, p->paletteTexture_);

   glBindVertexArray(p->vao_);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, p->numVertices_);

   SCWX_GL_CHECK_ERROR();
}

void GribProductLayer::Deinitialize()
{
   logger_->debug("Deinitialize()");

   delete p->reloadTimer_;
   p->reloadTimer_ = nullptr;

   glDeleteVertexArrays(1, &p->vao_);
   glDeleteBuffers(1, &p->vbo_);
   glDeleteTextures(1, &p->dataTexture_);
   glDeleteTextures(1, &p->paletteTexture_);

   p->vao_            = GL_INVALID_INDEX;
   p->vbo_            = GL_INVALID_INDEX;
   p->dataTexture_    = GL_INVALID_INDEX;
   p->paletteTexture_ = GL_INVALID_INDEX;
   p->frameLoaded_    = false;
   p->numVertices_    = 0;
}

std::optional<float> GribProductLayer::ValueAt(double lat, double lon) const
{
   if (p->values_.empty() || p->nx_ <= 0 || p->ny_ <= 0)
   {
      return std::nullopt;
   }

   long gi = 0; // column, west to east, matching the wire format's own
   long gj = 0; // row-major (index = row * nx_ + col) scan order

   if (p->gridType_ == GridType::RegularLatLon)
   {
      // lat1_/lon1_ is the north-west corner; grid scans east (+di_) and
      // south (-dj_) -- see LoadFrame's mesh-building comment.
      gi = std::lround((lon - p->lon1_) / p->di_);
      gj = std::lround((p->lat1_ - lat) / p->dj_);
   }
   else // Lambert
   {
      // Exact inverse of LambertGridToLatLon: reuses the same
      // ComputeLambertConstants/LambertForward this file already uses to
      // build the mesh, just run lat/lon -> grid index instead of grid
      // index -> lat/lon.
      const LambertGrid      grid {p->lov_,
                                   p->lad_,
                                   p->latin1_,
                                   p->latin2_,
                                   p->lat1_,
                                   p->lon1_,
                                   p->dx_,
                                   p->dy_,
                                   p->radius_};
      const LambertConstants c = ComputeLambertConstants(grid);

      const glm::dvec2 origin = LambertForward(grid, c, p->lat1_, p->lon1_);
      const glm::dvec2 target = LambertForward(grid, c, lat, lon);

      gi = std::lround((target.x - origin.x) / p->dx_);
      gj = std::lround((target.y - origin.y) / p->dy_);
   }

   if (gi < 0 || gi >= p->nx_ || gj < 0 || gj >= p->ny_)
   {
      return std::nullopt;
   }

   const float value =
      p->values_[static_cast<size_t>(gj) * static_cast<size_t>(p->nx_) +
                 static_cast<size_t>(gi)];

   // Same comparison the fragment shader itself uses to discard (see
   // grib.frag) -- keeps the tooltip in agreement with what's actually
   // rendered, rather than a separate, possibly-differing notion of "no
   // data here".
   if (value < p->noDataThreshold_)
   {
      return std::nullopt;
   }

   return value;
}

std::optional<std::string> GribProductLayer::GetHoverText(
   const std::shared_ptr<MapContext>& /* mapContext */,
   const common::Coordinate& mouseGeoCoords) const
{
   if (!p->frameLoaded_)
   {
      return std::nullopt;
   }

   std::optional<float> value =
      ValueAt(mouseGeoCoords.latitude_, mouseGeoCoords.longitude_);
   if (!value.has_value())
   {
      return std::nullopt;
   }

   const std::string formattedValue =
      p->gribManager_ ? p->gribManager_->FormatValue(value.value()) :
                        fmt::format("{:.2f}", value.value());

   return fmt::format(
      "{}\n{}\nValid: {}", p->productLabel_, formattedValue, p->validTime_);
}

bool GribProductLayer::RunMousePicking(
   const std::shared_ptr<MapContext>& mapContext,
   const QMapLibre::CustomLayerRenderParameters& /* params */,
   const QPointF& /* mouseLocalPos */,
   const QPointF& mouseGlobalPos,
   const glm::vec2& /* mouseCoords */,
   const common::Coordinate& mouseGeoCoords,
   std::shared_ptr<types::EventHandler>& /* eventHandler */)
{
   // Shift-gated, matching the app-wide convention established by
   // RadarProductLayer/RadarSiteLayer: that layer shows nothing at all
   // without Shift, and shows the actual data value under the cursor (plus
   // a distance-from-radar-site measurement, which has no equivalent here)
   // when it's held. Previously this layer showed an unconditional
   // product/time label instead of gating on Shift at all -- that
   // undersold what Shift actually means elsewhere in the app: "show me
   // the data", not just "a modifier radar happens to use".
   if (!(QGuiApplication::keyboardModifiers() &
         Qt::KeyboardModifier::ShiftModifier))
   {
      return false;
   }

   // Combines this layer's own GetHoverText() with every "area" layer
   // wired to it via AddAreaSibling() (see MapWidgetImpl::AddLayer) --
   // e.g. MRMS reflectivity, Models temperature, and radar's own sweep
   // value all in one tooltip. Scoped to just these layers peeking at
   // each other directly; the app's general mouse-picking dispatch
   // (MapWidgetImpl::RunMousePicking, used by polygons/markers/alerts
   // too) still stops at the first hit -- not touched here.
   std::optional<std::string> hoverText =
      CombineAreaHoverText(mapContext, mouseGeoCoords);
   if (!hoverText.has_value())
   {
      return false;
   }

   util::tooltip::Show(*hoverText, mouseGlobalPos);
   return true;
}

} // namespace scwx::qt::map
