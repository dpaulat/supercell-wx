#include <scwx/qt/view/radar_view.hpp>
#include <scwx/common/constants.hpp>

#include <boost/log/trivial.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ = "[scwx::qt::view::radar_view] ";

static constexpr uint32_t VERTICES_PER_BIN  = 6;
static constexpr uint32_t VALUES_PER_VERTEX = 2;

static std::chrono::system_clock::time_point
TimePoint(uint16_t modifiedJulianDate, uint32_t milliseconds);

class RadarViewImpl
{
public:
   explicit RadarViewImpl(std::shared_ptr<manager::RadarManager> radarManager,
                          std::shared_ptr<QMapboxGL>             map) :
       radarManager_(radarManager),
       map_(map),
       plotTime_(),
       colorTable_ {boost::gil::rgba8_pixel_t(0, 128, 0, 255),
                    boost::gil::rgba8_pixel_t(255, 192, 0, 255),
                    boost::gil::rgba8_pixel_t(255, 0, 0, 255)}
   {
   }
   ~RadarViewImpl() = default;

   std::shared_ptr<manager::RadarManager> radarManager_;
   std::shared_ptr<QMapboxGL>             map_;

   std::vector<float>    vertices_;
   std::vector<uint8_t>  dataMoments8_;
   std::vector<uint16_t> dataMoments16_;

   std::chrono::system_clock::time_point plotTime_;

   std::vector<boost::gil::rgba8_pixel_t> colorTable_;
};

RadarView::RadarView(std::shared_ptr<manager::RadarManager> radarManager,
                     std::shared_ptr<QMapboxGL>             map) :
    p(std::make_unique<RadarViewImpl>(radarManager, map))
{
   connect(radarManager.get(),
           &manager::RadarManager::Level2DataLoaded,
           this,
           &RadarView::UpdatePlot);
}
RadarView::~RadarView() = default;

double RadarView::bearing() const
{
   return p->map_->bearing();
}

double RadarView::scale() const
{
   return p->map_->scale();
}

const std::vector<uint8_t>& RadarView::data_moments8() const
{
   return p->dataMoments8_;
}

const std::vector<uint16_t>& RadarView::data_moments16() const
{
   return p->dataMoments16_;
}

const std::vector<float>& RadarView::vertices() const
{
   return p->vertices_;
}

const std::vector<boost::gil::rgba8_pixel_t>& RadarView::color_table() const
{
   return p->colorTable_;
}

void RadarView::Initialize()
{
   UpdatePlot();
}

void RadarView::LoadColorTable(std::shared_ptr<common::ColorTable> colorTable)
{
   // TODO: Make size, offset and scale dynamic
   const float offset = 66.0f;
   const float scale  = 2.0f;

   std::vector<boost::gil::rgba8_pixel_t>& lut = p->colorTable_;
   lut.resize(254);

   auto dataRange = boost::irange<uint16_t>(2, 255);

   std::for_each(std::execution::par_unseq,
                 dataRange.begin(),
                 dataRange.end(),
                 [&](uint16_t i) {
                    float f                     = (i - offset) / scale;
                    lut[i - *dataRange.begin()] = colorTable->Color(f);
                 });

   emit ColorTableLoaded();
}

std::chrono::system_clock::time_point RadarView::PlotTime()
{
   return p->plotTime_;
}

void RadarView::UpdatePlot()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "UpdatePlot()";

   boost::timer::cpu_timer timer;

   // TODO: Pick this based on radar data
   const std::vector<float>& coordinates =
      p->radarManager_->coordinates(common::RadialSize::_0_5Degree);

   std::shared_ptr<const wsr88d::Ar2vFile> level2Data =
      p->radarManager_->level2_data();
   if (level2Data == nullptr)
   {
      return;
   }

   // TODO: Pick these based on view settings
   auto                       radarData = level2Data->radar_data()[0];
   wsr88d::rda::DataBlockType blockType = wsr88d::rda::DataBlockType::MomentRef;

   p->plotTime_ = TimePoint(radarData[0]->modified_julian_date(),
                            radarData[0]->collection_time());

   // Calculate vertices
   timer.start();

   auto momentData0 = radarData[0]->moment_data_block(blockType);

   // Setup vertex vector
   std::vector<float>& vertices = p->vertices_;
   const size_t        radials  = radarData.size();
   const uint32_t      gates    = momentData0->number_of_data_moment_gates();
   size_t              vIndex   = 0;
   vertices.clear();
   vertices.resize(radials * gates * VERTICES_PER_BIN * VALUES_PER_VERTEX);

   // Setup data moment vector
   std::vector<uint8_t>&  dataMoments8  = p->dataMoments8_;
   std::vector<uint16_t>& dataMoments16 = p->dataMoments16_;
   size_t                 mIndex        = 0;

   if (momentData0->data_word_size() == 8)
   {
      dataMoments16.resize(0);
      dataMoments16.shrink_to_fit();

      dataMoments8.resize(radials * gates * VERTICES_PER_BIN);
   }
   else
   {
      dataMoments8.resize(0);
      dataMoments8.shrink_to_fit();

      dataMoments16.resize(radials * gates * VERTICES_PER_BIN);
   }

   // Compute threshold at which to display an individual bin
   const float    scale  = momentData0->scale();
   const float    offset = momentData0->offset();
   const uint16_t snrThreshold =
      std::lroundf(momentData0->snr_threshold_raw() * scale / 10 + offset);

   // Azimuth resolution spacing:
   //   1 = 0.5 degrees
   //   2 = 1.0 degrees
   const float radialMultiplier =
      2.0f /
      std::clamp<int8_t>(radarData[0]->azimuth_resolution_spacing(), 1, 2);

   const float    startAngle  = radarData[0]->azimuth_angle();
   const uint16_t startRadial = std::lroundf(startAngle * radialMultiplier);

   for (uint16_t radial = 0; radial < radials; ++radial)
   {
      auto radialData = radarData[radial];
      auto momentData = radarData[radial]->moment_data_block(blockType);

      if (momentData0->data_word_size() != momentData->data_word_size())
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Radial " << radial << " has different word size";
         continue;
      }

      // Compute gate interval
      const uint16_t dataMomentRange = momentData->data_moment_range_raw();
      const uint16_t dataMomentInterval =
         momentData->data_moment_range_sample_interval_raw();
      const uint16_t dataMomentIntervalH = dataMomentInterval / 2;

      // Compute gate size (number of base 250m gates per bin)
      const uint16_t gateSize = std::max<uint16_t>(1, dataMomentInterval / 250);

      // Compute gate range [startGate, endGate)
      const uint16_t startGate = (dataMomentRange - dataMomentIntervalH) / 250;
      const uint16_t numberOfDataMomentGates =
         std::min<uint16_t>(momentData->number_of_data_moment_gates(),
                            static_cast<uint16_t>(gates));
      const uint16_t endGate =
         std::min<uint16_t>(startGate + numberOfDataMomentGates * gateSize,
                            common::MAX_DATA_MOMENT_GATES);

      const uint8_t*  dataMomentsArray8  = nullptr;
      const uint16_t* dataMomentsArray16 = nullptr;

      if (momentData->data_word_size() == 8)
      {
         dataMomentsArray8 =
            reinterpret_cast<const uint8_t*>(momentData->data_moments());
      }
      else
      {
         dataMomentsArray16 =
            reinterpret_cast<const uint16_t*>(momentData->data_moments());
      }

      for (uint16_t gate = startGate, i = 0; gate + gateSize <= endGate;
           gate += gateSize, ++i)
      {
         size_t vertexCount = (gate > 0) ? 6 : 3;

         // Store data moment value
         if (dataMomentsArray8 != nullptr)
         {
            uint8_t dataValue = dataMomentsArray8[i];
            if (dataValue < snrThreshold)
            {
               continue;
            }

            for (size_t m = 0; m < vertexCount; m++)
            {
               dataMoments8[mIndex++] = dataMomentsArray8[i];
            }
         }
         else
         {
            uint16_t dataValue = dataMomentsArray16[i];
            if (dataValue < snrThreshold)
            {
               continue;
            }

            for (size_t m = 0; m < vertexCount; m++)
            {
               dataMoments16[mIndex++] = dataMomentsArray16[i];
            }
         }

         // Store vertices
         if (gate > 0)
         {
            const uint16_t baseCoord = gate - 1;

            size_t offset1 = ((startRadial + radial) % common::MAX_RADIALS *
                                 common::MAX_DATA_MOMENT_GATES +
                              baseCoord) *
                             2;
            size_t offset2 = offset1 + gateSize * 2;
            size_t offset3 =
               (((startRadial + radial + 1) % common::MAX_RADIALS) *
                   common::MAX_DATA_MOMENT_GATES +
                baseCoord) *
               2;
            size_t offset4 = offset3 + gateSize * 2;

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];

            vertices[vIndex++] = coordinates[offset3];
            vertices[vIndex++] = coordinates[offset3 + 1];

            vertices[vIndex++] = coordinates[offset3];
            vertices[vIndex++] = coordinates[offset3 + 1];

            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];

            vertexCount = 6;
         }
         else
         {
            const uint16_t baseCoord = gate;

            size_t offset1 = ((startRadial + radial) % common::MAX_RADIALS *
                                 common::MAX_DATA_MOMENT_GATES +
                              baseCoord) *
                             2;
            size_t offset2 =
               (((startRadial + radial + 1) % common::MAX_RADIALS) *
                   common::MAX_DATA_MOMENT_GATES +
                baseCoord) *
               2;

            // TODO: Radar location
            vertices[vIndex++] = 38.6986f;
            vertices[vIndex++] = -90.6828f;

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];

            vertexCount = 3;
         }
      }
   }
   vertices.resize(vIndex);

   if (momentData0->data_word_size() == 8)
   {
      dataMoments8.resize(mIndex);
   }
   else
   {
      dataMoments16.resize(mIndex);
   }

   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Vertices calculated in " << timer.format(6, "%ws");

   emit PlotUpdated();
}

static std::chrono::system_clock::time_point
TimePoint(uint16_t modifiedJulianDate, uint32_t milliseconds)
{
   using namespace std::chrono;
   using sys_days       = time_point<system_clock, days>;
   constexpr auto epoch = sys_days {1969y / December / 31d};

   return epoch + (modifiedJulianDate * 24h) +
          std::chrono::milliseconds {milliseconds};
}

} // namespace view
} // namespace qt
} // namespace scwx
