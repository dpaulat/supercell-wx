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

   const std::vector<float>& coordinates =
      p->radarManager_->coordinates(common::RadialSize::_0_5Degree);

   // Calculate vertices
   timer.start();
   std::vector<float>& vertices = p->vertices_;
   const uint32_t      radials  = common::MAX_RADIALS;
   const uint32_t      gates    = common::MAX_DATA_MOMENT_GATES;
   vertices.clear();
   vertices.resize(radials * gates * VERTICES_PER_BIN * VALUES_PER_VERTEX);
   size_t index = 0;

   for (uint16_t radial = 0; radial < 720; ++radial)
   {
      const float dataMomentRange     = 2.125f * 1000.0f;
      const float dataMomentInterval  = 0.25f * 1000.0f;
      const float dataMomentIntervalH = dataMomentInterval * 0.5f;
      const float snrThreshold        = 2.0f;

      const uint16_t startGate               = 7;
      const uint16_t numberOfDataMomentGates = 1832;
      const uint16_t endGate =
         std::min<uint16_t>(numberOfDataMomentGates + startGate,
                            common::MAX_DATA_MOMENT_GATES - 1);

      for (uint16_t gate = startGate; gate < endGate; ++gate)
      {
         size_t offset1 = (radial * common::MAX_DATA_MOMENT_GATES + gate) * 2;
         size_t offset2 = offset1 + 2;
         size_t offset3 = (((radial + 1) % common::MAX_RADIALS) *
                              common::MAX_DATA_MOMENT_GATES +
                           gate) *
                          2;
         size_t offset4 = offset3 + 2;

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
   }
   timer.stop();
   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Vertices calculated in " << timer.format(6, "%ws");
}

} // namespace view
} // namespace qt
} // namespace scwx
