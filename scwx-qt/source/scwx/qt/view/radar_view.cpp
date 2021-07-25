#include <scwx/qt/view/radar_view.hpp>
#include <scwx/common/constants.hpp>

#include <boost/log/trivial.hpp>
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

class RadarViewImpl
{
public:
   explicit RadarViewImpl(std::shared_ptr<manager::RadarManager> radarManager,
                          std::shared_ptr<QMapboxGL>             map) :
       radarManager_(radarManager), map_(map)
   {
   }
   ~RadarViewImpl() = default;

   std::shared_ptr<manager::RadarManager> radarManager_;
   std::shared_ptr<QMapboxGL>             map_;

   std::vector<float> vertices_;
};

RadarView::RadarView(std::shared_ptr<manager::RadarManager> radarManager,
                     std::shared_ptr<QMapboxGL>             map) :
    p(std::make_unique<RadarViewImpl>(radarManager, map))
{
}
RadarView::~RadarView() = default;

RadarView::RadarView(RadarView&&) noexcept = default;
RadarView& RadarView::operator=(RadarView&&) noexcept = default;

double RadarView::bearing() const
{
   return p->map_->bearing();
}

double RadarView::scale() const
{
   return p->map_->scale();
}

const std::vector<float>& RadarView::vertices() const
{
   return p->vertices_;
}

void RadarView::Initialize()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Initialize()";

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

   // Calculate vertices
   timer.start();

   auto momentData0 = radarData[0]->moment_data_block(blockType);

   std::vector<float>& vertices = p->vertices_;
   const size_t        radials  = radarData.size();
   const uint32_t      gates    = momentData0->number_of_data_moment_gates();
   vertices.clear();
   vertices.resize(radials * gates * VERTICES_PER_BIN * VALUES_PER_VERTEX);
   size_t index = 0;

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

      const uint8_t*  dataMoments8  = nullptr;
      const uint16_t* dataMoments16 = nullptr;

      if (momentData->data_word_size() == 8)
      {
         dataMoments8 =
            reinterpret_cast<const uint8_t*>(momentData->data_moments());
      }
      else
      {
         dataMoments16 =
            reinterpret_cast<const uint16_t*>(momentData->data_moments());
      }

      for (uint16_t gate = startGate, i = 0; gate + gateSize <= endGate;
           gate += gateSize, ++i)
      {
         uint16_t dataValue =
            (dataMoments8 != nullptr) ? dataMoments8[i] : dataMoments16[i];

         if (dataValue < snrThreshold)
         {
            continue;
         }

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

            vertices[index++] = coordinates[offset1];
            vertices[index++] = coordinates[offset1 + 1];

            vertices[index++] = coordinates[offset2];
            vertices[index++] = coordinates[offset2 + 1];

            vertices[index++] = coordinates[offset3];
            vertices[index++] = coordinates[offset3 + 1];

            vertices[index++] = coordinates[offset3];
            vertices[index++] = coordinates[offset3 + 1];

            vertices[index++] = coordinates[offset4];
            vertices[index++] = coordinates[offset4 + 1];

            vertices[index++] = coordinates[offset2];
            vertices[index++] = coordinates[offset2 + 1];
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
            vertices[index++] = 38.6986f;
            vertices[index++] = -90.6828f;

            vertices[index++] = coordinates[offset1];
            vertices[index++] = coordinates[offset1 + 1];

            vertices[index++] = coordinates[offset2];
            vertices[index++] = coordinates[offset2 + 1];
         }
      }
   }
   vertices.resize(index);
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Vertices calculated in " << timer.format(6, "%ws");
}

} // namespace view
} // namespace qt
} // namespace scwx
