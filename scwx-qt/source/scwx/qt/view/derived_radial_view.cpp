#include "scwx/qt/types/unit_types.hpp"
#include <scwx/qt/view/derived_radial_view.hpp>

#include <scwx/common/constants.hpp>
#include <scwx/deriver/base_deriver.hpp>
#include <scwx/deriver/data/derived_radial_data.hpp>
#include <scwx/deriver/deriver_factory.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/qt/util/geographic_lib.hpp>

#include <limits>

#include <GeographicLib/Geodesic.hpp>

#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <utility>

namespace scwx::qt::view
{

static const std::string logPrefix_ = "scwx::qt::view::derived_radial_view";
static const auto        logger_    = scwx::util::Logger::Create(logPrefix_);

static constexpr std::uint32_t kMaxRadialGates_ =
   common::MAX_0_5_DEGREE_RADIALS * common::MAX_DATA_MOMENT_GATES;
static constexpr std::uint32_t kMaxCoordinates_ = kMaxRadialGates_ * 2u;

static constexpr uint16_t      RANGE_FOLDED      = 1u;
static constexpr std::uint32_t VERTICES_PER_BIN  = 6u;
static constexpr std::uint32_t VALUES_PER_VERTEX = 2u;

static constexpr size_t TIME_DIGITS = 6u;

class DerivedRadialView::Impl
{
public:
   Impl(DerivedRadialView* self, std::string product) :
       self_ {self},
       product_ {std::move(product)},
       deriver_ {deriver::DeriverFactory::CreateDeriver(product_)},
       colorTable_ {},
       colorTableLut_ {},
       savedColorTable_ {nullptr},
       savedScale_ {0.0f}
   {
   }

   void ComputeCoordinates(bool smoothingEnabled, float gateSize);

   const DerivedRadialView* self_;

   std::string                           product_;
   std::shared_ptr<deriver::BaseDeriver> deriver_;

   std::shared_ptr<deriver::data::DerivedRadialData> radialData_ {nullptr};

   std::shared_ptr<common::ColorTable>    colorTable_;
   std::vector<boost::gil::rgba8_pixel_t> colorTableLut_;
   uint16_t                               colorTableMin_ {};
   uint16_t                               colorTableMax_ {};

   uint16_t                            threshold_ {2};
   std::shared_ptr<common::ColorTable> savedColorTable_;
   float                               savedScale_ {1.0f};
   float                               savedOffset_ {0.0f};
   uint16_t                            savedLogStart_ {1u};
   float                               savedLogScale_ {1.0f};
   float                               savedLogOffset_ {0.0f};

   float latitude_ {0.0f};
   float longitude_ {0.0f};

   std::vector<float>        coordinates_ {};
   std::vector<float>        vertices_ {};
   std::vector<std::uint8_t> dataMoments8_ {};

   types::SpeedUnits speedUnits_ = {types::SpeedUnits::Knots};

   boost::asio::thread_pool threadPool_ {1};
};

DerivedRadialView::DerivedRadialView(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    RadarProductView(std::move(radarProductManager)),
    p {std::make_unique<Impl>(this, product)}
{
   ConnectRadarProductManager();
}

DerivedRadialView::~DerivedRadialView() = default;

std::optional<float> DerivedRadialView::elevation() const
{
   if (p->radialData_ == nullptr || !p->radialData_->meta_data().hasElevation)
   {
      return std::nullopt;
   }
   else
   {
      return static_cast<float>(p->radialData_->meta_data().elevation.value());
   }
}

float DerivedRadialView::range() const
{
   if (p->radialData_ == nullptr)
   {
      return 0.0;
   }

   return p->radialData_->meta_data().range.value();
}

std::chrono::system_clock::time_point DerivedRadialView::sweep_time() const
{
   if (p->radialData_ == nullptr)
   {
      return {};
   }
   return p->radialData_->meta_data().sweepTime;
}

uint16_t DerivedRadialView::vcp() const
{
   if (p->radialData_ == nullptr)
   {
      return 0;
   }
   return p->radialData_->meta_data().vcp;
}

const std::vector<float>& DerivedRadialView::vertices() const
{
   return p->vertices_;
}

void DerivedRadialView::ConnectRadarProductManager()
{
   connect(radar_product_manager().get(),
           &manager::RadarProductManager::DataReloaded,
           this,
           [this](const std::shared_ptr<types::RadarProductRecord>& record)
           {
              const auto& derivableProducts =
                 deriver::DeriverFactory::GetDeriveableProducts(p->product_);
              const auto& productInfoIt = derivableProducts.find(p->product_);
              if (productInfoIt == derivableProducts.cend() ||
                  p->deriver_ == nullptr)
              {
                 return;
              }
              const auto& productInfo = productInfoIt->second;
              if (record->radar_product_group() ==
                  common::RadarProductGroup::Level3)
              {
                 const auto& iter =
                    std::find_if(productInfo.level3AwipsIds_.cbegin(),
                                 productInfo.level3AwipsIds_.cend(),
                                 [record](const auto& item)
                                 { return item == record->radar_product(); });
                 if (iter != productInfo.level3AwipsIds_.cend())
                 {
                    Update();
                 }
              }
              else if (record->radar_product_group() ==
                          common::RadarProductGroup::Level2 &&
                       !productInfo.level2Products_.empty())
              {
                 Update();
              }
           });
}

void DerivedRadialView::DisconnectRadarProductManager()
{
   disconnect(radar_product_manager().get(),
              &manager::RadarProductManager::DataReloaded,
              this,
              nullptr);
}

std::shared_ptr<common::ColorTable> DerivedRadialView::color_table() const
{
   return p->colorTable_;
}

const std::vector<boost::gil::rgba8_pixel_t>&
DerivedRadialView::color_table_lut() const
{
   if (p->colorTableLut_.size() == 0)
   {
      return RadarProductView::color_table_lut();
   }
   else
   {
      return p->colorTableLut_;
   }
}

uint16_t DerivedRadialView::color_table_min() const
{
   if (p->colorTableLut_.size() == 0)
   {
      return RadarProductView::color_table_min();
   }
   else
   {
      return p->colorTableMin_;
   }
}

uint16_t DerivedRadialView::color_table_max() const
{
   if (p->colorTableLut_.size() == 0)
   {
      return RadarProductView::color_table_max();
   }
   else
   {
      return p->colorTableMax_;
   }
}

bool DerivedRadialView::IgnoreUnits() const
{
   // TODO
   if (p->product_.starts_with("SRV"))
   {
      return false;
   }
   return true;
}

float DerivedRadialView::unit_scale() const
{
   // TODO I still need to do this
   if (p->product_.starts_with("SRV"))
   {
      return types::GetSpeedUnitsScale(p->speedUnits_);
   }

   return std::numeric_limits<float>().quiet_NaN();
}

std::string DerivedRadialView::units() const
{
   // TODO I still need to do this
   if (p->product_.starts_with("SRV"))
   {
      return types::GetSpeedUnitsAbbreviation(p->speedUnits_);
   }
   return "";
}

common::RadarProductGroup DerivedRadialView::GetRadarProductGroup() const
{
   // TODO I still need to do this
   return common::RadarProductGroup::Derived;
}

std::string DerivedRadialView::GetRadarProductName() const
{
   return p->product_;
}

void DerivedRadialView::SelectProduct(const std::string& productName)
{
   p->product_ = productName;
   p->deriver_ = deriver::DeriverFactory::CreateDeriver(p->product_);
}

std::vector<std::pair<std::string, std::string>>
DerivedRadialView::GetDescriptionFields() const
{
   // TODO I still need to do this
   return {};
}

void DerivedRadialView::LoadColorTable(
   std::shared_ptr<common::ColorTable> colorTable)
{
   p->colorTable_ = colorTable;
   UpdateColorTableLut();
}

void DerivedRadialView::UpdateColorTableLut()
{
   logger_->debug("UpdateColorTable()");

   if (p->radialData_ == nullptr || p->colorTable_ == nullptr ||
       !p->colorTable_->IsValid())
   {
      return;
   }

   const auto& metaData = p->radialData_->meta_data();

   const float   offset    = metaData.offset;
   const float   scale     = metaData.scale;
   const uint8_t threshold = metaData.threshold;

   // If the threshold is 2, the range min should be set to 1 for range
   // folding
   const uint8_t  rangeMin       = std::min<std::uint8_t>(1, threshold);
   const uint16_t numberOfLevels = metaData.numberOfLevels;
   const uint8_t  rangeMax       = static_cast<std::uint8_t>(
      std::clamp<std::uint16_t>((numberOfLevels > 0) ? numberOfLevels - 1 : 0,
                                std::numeric_limits<std::uint8_t>::min(),
                                std::numeric_limits<std::uint8_t>::max()));

   if (p->savedColorTable_ == p->colorTable_ && //
       p->savedOffset_ == offset &&             //
       p->savedScale_ == scale &&               //
       numberOfLevels > 16) // NOLINT always rebuild 16 level products
   {
      return;
   }

   // Iterate over [rangeMin, numberOfLevels)
   boost::integer_range<uint16_t> dataRange =
      boost::irange<uint16_t>(rangeMin, numberOfLevels);

   std::vector<boost::gil::rgba8_pixel_t>& lut = p->colorTableLut_;
   lut.resize(numberOfLevels - rangeMin);
   lut.shrink_to_fit();

   p->colorTableMin_ = rangeMin;
   p->colorTableMax_ = rangeMax;

   p->threshold_       = threshold;
   p->savedColorTable_ = p->colorTable_;
   p->savedOffset_     = offset;
   p->savedScale_      = scale;

   std::for_each(std::execution::par_unseq,
                 dataRange.begin(),
                 dataRange.end(),
                 [&](uint16_t i)
                 {
                    const size_t lutIndex = i - *dataRange.begin();

                    if (i == RANGE_FOLDED && threshold > RANGE_FOLDED)
                    {
                       lut[lutIndex] = p->colorTable_->rf_color();
                    }
                    else
                    {
                       std::optional<float> f = GetDataValue(i);
                       if (f.has_value())
                       {
                          lut[lutIndex] = p->colorTable_->Color(f.value());
                       }
                       else
                       {
                          lut[lutIndex] =
                             boost::gil::rgba8_pixel_t {0, 0, 0, 0};
                       }
                    }
                 });

   Q_EMIT ColorTableLutUpdated();
}

std::optional<wsr88d::DataLevelCode>
DerivedRadialView::GetDataLevelCode(std::uint16_t level) const
{
   if (level > p->threshold_)
   {
      return std::nullopt;
   }
   else if (level == RANGE_FOLDED)
   {
      return wsr88d::DataLevelCode::RangeFolded;
   }
   else
   {
      return wsr88d::DataLevelCode::BelowThreshold;
   }
}

std::optional<float> DerivedRadialView::GetDataValue(std::uint16_t level) const
{
   if (level <= p->threshold_)
   {
      return std::nullopt;
   }

   return static_cast<float>(level - p->threshold_) * p->savedScale_ +
          p->savedOffset_;
}

std::tuple<const void*, size_t, size_t> DerivedRadialView::GetMomentData() const
{
   const void* data          = p->dataMoments8_.data();
   size_t      dataSize      = p->dataMoments8_.size() * sizeof(uint8_t);
   size_t      componentSize = 1;

   return std::tie(data, dataSize, componentSize);
}

void DerivedRadialView::ComputeSweep()
{
   logger_->trace("ComputeSweep()");

   boost::timer::cpu_timer timer;

   const std::scoped_lock sweepLock(sweep_mutex());

   const std::shared_ptr<manager::RadarProductManager> radarProductManager =
      radar_product_manager();

   const auto& derivableProducts =
      deriver::DeriverFactory::GetDeriveableProducts(p->product_);
   const auto& productInfoIt = derivableProducts.find(p->product_);
   if (productInfoIt == derivableProducts.cend() || p->deriver_ == nullptr)
   {
      Q_EMIT SweepNotComputed(types::NoUpdateReason::NotLoaded);
      return;
   }
   const auto& productInfo = productInfoIt->second;

   bool hasNewData = false;

   for (const auto& neededL3Product : productInfo.level3AwipsIds_)
   {
      const auto [data, time] =
         radarProductManager->GetLevel3Data(neededL3Product, selected_time());
      hasNewData =
         p->deriver_->SetLevel3Input(neededL3Product, data) || hasNewData;
   }

   for (const auto& neededL2Product : productInfo.level2Products_)
   {
      const auto& [dataBlockType, elevation] = neededL2Product;
      const auto [data, elevationGot, elecationCuts, time] =
         radarProductManager->GetLevel2Data(
            dataBlockType, elevation, selected_time());
      hasNewData =
         p->deriver_->SetLevel2Input(dataBlockType, elevation, data) ||
         hasNewData;
   }

   if (!hasNewData)
   {
      // No need to recalculate
      Q_EMIT SweepNotComputed(types::NoUpdateReason::NoChange);
      return;
   }

   timer.start();
   auto derivedData = p->deriver_->GetOutput(p->product_);
   timer.stop();
   logger_->debug("Product derived in {}", timer.format(TIME_DIGITS, "%ws"));
   if (derivedData == nullptr)
   {
      // Data was not avalible. This may be do to lack if input data, or an
      // error. Any error should be signaled in the deriver.
      Q_EMIT SweepNotComputed(types::NoUpdateReason::NotLoaded);
      return;
   }
   p->radialData_ =
      std::dynamic_pointer_cast<deriver::data::DerivedRadialData>(derivedData);
   if (p->radialData_ == nullptr)
   {
      logger_->error("Deriver did not return radial data.");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   // TODO get new product data from radar manager (not done yet)
   const bool smoothingEnabled         = false;
   const bool showSmoothedRangeFolding = false;
   // There is a lot that goes here that is TODO

   logger_->debug("Computing Sweep");

   auto& radialData = p->radialData_;
   // Valid number of radials is 1-720
   size_t radials = radialData->radials();
   // This should never have more than 720 radials
   // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
   if (radials < 1 || radials > 720)
   {
      logger_->warn("Unsupported number of radials: {}", radials);
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   common::RadialSize radialSize = common::RadialSize::NonStandard;
   if (radarProductManager->is_tdwr())
   {
      radialSize = common::RadialSize::NonStandard;
   }
   else
   {
      if (radials == common::MAX_0_5_DEGREE_RADIALS)
      {
         radialSize = common::RadialSize::_0_5Degree;
      }
      else if (radials == common::MAX_1_DEGREE_RADIALS)
      {
         radialSize = common::RadialSize::_1Degree;
      }
      else
      {
         radialSize = common::RadialSize::NonStandard;
      }
   }

   const std::vector<float>& coordinates =
      (radialSize == common::RadialSize::NonStandard) ?
         p->coordinates_ :
         radarProductManager->coordinates(radialSize, smoothingEnabled);

   // There should be a positive number of range bins in radial data
   const size_t numberOfDataMomentGates = radialData->gates();
   if (numberOfDataMomentGates < 1)
   {
      logger_->warn("No range bins in radial data");
      Q_EMIT SweepNotComputed(types::NoUpdateReason::InvalidData);
      return;
   }

   // Calculate vertices
   timer.start();

   // Setup vertex vector
   std::vector<float>& vertices = p->vertices_;
   size_t              vIndex   = 0;
   vertices.clear();
   vertices.resize(radials * numberOfDataMomentGates * VERTICES_PER_BIN *
                   VALUES_PER_VERTEX);

   // Setup data moment vector
   std::vector<uint8_t>& dataMoments8 = p->dataMoments8_;
   size_t                mIndex       = 0;
   dataMoments8.resize(radials * numberOfDataMomentGates * VERTICES_PER_BIN);

   // Compute threshold at which to display an individual bin
   const uint16_t snrThreshold = radialData->meta_data().threshold;

   // Compute gate interval
   const float dataMomentInterval =
      radialData->meta_data().dataMomentInterval.value();

   // Get the gate length in meters. Use dataMomentInterval for NonStandard to
   // avoid generating >1 base gates per bin.
   const float gateLength = radialSize == common::RadialSize::NonStandard ?
                               dataMomentInterval :
                               radarProductManager->gate_size();

   // Determine which radial to start at
   uint16_t startRadial = 0;
   if (radialSize == common::RadialSize::NonStandard)
   {
      p->ComputeCoordinates(smoothingEnabled, gateLength);
      startRadial = 0;
   }
   else
   {
      const float radialMultiplier = static_cast<float>(radials) / 360.0f;
      const float startAngle       = radialData->start_angle(0);
      startRadial = std::lroundf(startAngle * radialMultiplier);
   }

   // Compute gate size (number of base gates per bin)
   const uint16_t gateSize = std::max<uint16_t>(
      1, static_cast<uint16_t>(dataMomentInterval / gateLength));

   // Compute gate range [startGate, endGate)
   size_t       startGate = 0;
   const size_t endGate =
      std::min<std::size_t>(startGate + numberOfDataMomentGates * gateSize,
                            common::MAX_DATA_MOMENT_GATES);

   if (smoothingEnabled)
   {
      // If smoothing is enabled, the start gate is incremented by one, as we
      // are skipping the radar site origin. The end gate is unaffected, as
      // we need to draw one less data point.
      ++startGate;

      // For most products other than reflectivity, the edge should not go to
      // the bottom of the color table
      // Another TODO
      // p->edgeValue_ = ComputeEdgeValue();
   }

   auto         radarSite      = radarProductManager->radar_site();
   const double radarLatitude  = radarSite->latitude();
   const double radarLongitude = radarSite->longitude();

   for (size_t radial = 0; radial < radialData->radials(); ++radial)
   {
      const auto&  dataMomentsArray8 = radialData->levels(radial);
      const size_t nextRadial =
         (radial == radialData->radials() - 1) ? 0 : radial + 1;
      const auto& nextDataMomentsArray8 = radialData->levels(nextRadial);

      for (size_t gate = startGate, i = 0; gate + gateSize <= endGate;
           gate += gateSize, ++i)
      {
         // TODO I think this is right?
         const size_t vertexCount =
            (gate > 0) ? VERTICES_PER_BIN : VERTICES_PER_BIN / 2;

         if (!smoothingEnabled)
         {
            // Store data moment value
            const uint8_t dataValue =
               (i < dataMomentsArray8.size()) ? dataMomentsArray8[i] : 0;
            if (dataValue < snrThreshold && dataValue != RANGE_FOLDED)
            {
               continue;
            }

            for (size_t m = 0; m < vertexCount; m++)
            {
               dataMoments8[mIndex++] = dataValue;
            }
         }
         else if (gate > 0)
         {
            // Validate indices are all in range
            if (i + 1 >= numberOfDataMomentGates)
            {
               continue;
            }

            const std::uint8_t& dm1 = dataMomentsArray8[i];
            const std::uint8_t& dm2 = dataMomentsArray8[i + 1];
            const std::uint8_t& dm3 = nextDataMomentsArray8[i];
            const std::uint8_t& dm4 = nextDataMomentsArray8[i + 1];

            if ((!showSmoothedRangeFolding && //
                 (dm1 < snrThreshold || dm1 == RANGE_FOLDED) &&
                 (dm2 < snrThreshold || dm2 == RANGE_FOLDED) &&
                 (dm3 < snrThreshold || dm3 == RANGE_FOLDED) &&
                 (dm4 < snrThreshold || dm4 == RANGE_FOLDED)) ||
                (showSmoothedRangeFolding && //
                 dm1 < snrThreshold && dm1 != RANGE_FOLDED &&
                 dm2 < snrThreshold && dm2 != RANGE_FOLDED &&
                 dm3 < snrThreshold && dm3 != RANGE_FOLDED &&
                 dm4 < snrThreshold && dm4 != RANGE_FOLDED))
            {
               // Skip only if all data moments are hidden
               continue;
            }

            // The order must match the store vertices section below
            // TODO
            dataMoments8[mIndex++] = dm1; // p->RemapDataMoment(dm1);
            dataMoments8[mIndex++] = dm2; // p->RemapDataMoment(dm2);
            dataMoments8[mIndex++] = dm4; // p->RemapDataMoment(dm4);
            dataMoments8[mIndex++] = dm1; // p->RemapDataMoment(dm1);
            dataMoments8[mIndex++] = dm3; // p->RemapDataMoment(dm3);
            dataMoments8[mIndex++] = dm4; // p->RemapDataMoment(dm4);
         }
         else
         {
            // If smoothing is enabled, gate should never start at zero
            // (radar site origin)
            logger_->error("Smoothing enabled, gate should not start at zero");
            continue;
         }

         if (gate > 0)
         {
            const size_t baseCoord = gate - 1;

            const size_t offset1 = ((startRadial + radial) % radials *
                                       common::MAX_DATA_MOMENT_GATES +
                                    baseCoord) *
                                   2;
            const size_t offset2 = offset1 + static_cast<size_t>(gateSize) * 2;
            const size_t offset3 = (((startRadial + radial + 1) % radials) *
                                       common::MAX_DATA_MOMENT_GATES +
                                    baseCoord) *
                                   2;
            const size_t offset4 = offset3 + static_cast<size_t>(gateSize) * 2;

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];

            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset3];
            vertices[vIndex++] = coordinates[offset3 + 1];

            vertices[vIndex++] = coordinates[offset4];
            vertices[vIndex++] = coordinates[offset4 + 1];
         }
         else
         {
            const size_t baseCoord = gate;

            const size_t offset1 = ((startRadial + radial) % radials *
                                       common::MAX_DATA_MOMENT_GATES +
                                    baseCoord) *
                                   2;
            const size_t offset2 = (((startRadial + radial + 1) % radials) *
                                       common::MAX_DATA_MOMENT_GATES +
                                    baseCoord) *
                                   2;

            // TODO use coords from products
            vertices[vIndex++] = static_cast<float>(radarLatitude);
            vertices[vIndex++] = static_cast<float>(radarLongitude);

            vertices[vIndex++] = coordinates[offset1];
            vertices[vIndex++] = coordinates[offset1 + 1];

            vertices[vIndex++] = coordinates[offset2];
            vertices[vIndex++] = coordinates[offset2 + 1];
         }
      }
   }
   vertices.resize(vIndex);
   vertices.shrink_to_fit();

   dataMoments8.resize(mIndex);
   dataMoments8.shrink_to_fit();

   timer.stop();
   logger_->debug("Vertices calculated in {}",
                  timer.format(TIME_DIGITS, "%ws"));

   UpdateColorTableLut();

   Q_EMIT SweepComputed();
}

void DerivedRadialView::Impl::ComputeCoordinates(bool  smoothingEnabled,
                                                 float gateSize)
{
   logger_->debug("ComputeCoordinates()");
   boost::timer::cpu_timer timer;

   const GeographicLib::Geodesic& geodesic(
      util::GeographicLib::DefaultGeodesic());

   auto         radarProductManager = self_->radar_product_manager();
   auto         radarSite           = radarProductManager->radar_site();
   const double radarLatitude       = radarSite->latitude();
   const double radarLongitude      = radarSite->longitude();

   // Just double check radialData_
   if (radialData_ == nullptr)
   {
      return;
   }

   // Calculate azimuth coordinates
   timer.start();

   coordinates_.resize(kMaxCoordinates_);

   const size_t numRadials   = radialData_->radials();
   const size_t numRangeBins = radialData_->gates();

   auto radials = boost::irange<size_t>(0u, numRadials);
   auto gates   = boost::irange<size_t>(0u, numRangeBins);

   const float gateRangeOffset = (smoothingEnabled) ?
                                    // Center of the first gate is half the gate
                                    // size distance from the radar site
                                    0.5f :
                                    // Far end of the first gate is the gate
                                    // size distance from the radar site
                                    1.0f;

   std::for_each(
      std::execution::par_unseq,
      radials.begin(),
      radials.end(),
      [&](size_t radial)
      {
         float angle = radialData_->start_angle(radial);

         if (smoothingEnabled)
         {
            static constexpr float kDeltaAngleFactor = 0.5f;
            angle += radialData_->delta_angle(radial) * kDeltaAngleFactor;
         }

         std::for_each(
            std::execution::par_unseq,
            gates.begin(),
            gates.end(),
            [&](size_t gate)
            {
               const size_t radialGate =
                  radial * common::MAX_DATA_MOMENT_GATES + gate;
               const float range =
                  (static_cast<float>(gate) + gateRangeOffset) * gateSize;
               const size_t offset = radialGate * 2;
               if (offset + 1 >= coordinates_.size())
               {
                  return;
               }

               double latitude  = 0.0;
               double longitude = 0.0;

               geodesic.Direct(radarLatitude,
                               radarLongitude,
                               angle,
                               range,
                               latitude,
                               longitude);

               coordinates_[offset]     = static_cast<float>(latitude);
               coordinates_[offset + 1] = static_cast<float>(longitude);
            });
      });
   timer.stop();
   logger_->debug("Coordinates calculated in {}",
                  timer.format(TIME_DIGITS, "%ws"));
}

std::optional<std::uint16_t>
DerivedRadialView::GetBinLevel(const common::Coordinate& coordinate) const
{
   // Radar data is needed in order to give the bin level
   if (p->radialData_ == nullptr)
   {
      return std::nullopt;
   }

   auto         radarProductManager = radar_product_manager();
   auto         radarSite           = radarProductManager->radar_site();
   const double radarLatitude       = radarSite->latitude();
   const double radarLongitude      = radarSite->longitude();

   // Determine distance and azimuth of coordinate relative to radar location
   double s12  = 0; // Distance (meters)
   double azi1 = 0; // Azimuth (degrees)
   double azi2 = 0; // Unused
   util::GeographicLib::DefaultGeodesic().Inverse(radarLatitude,
                                                  radarLongitude,
                                                  coordinate.latitude_,
                                                  coordinate.longitude_,
                                                  s12,
                                                  azi1,
                                                  azi2);

   if (std::isnan(azi1))
   {
      // If a problem occurred with the geodesic inverse calculation
      return std::nullopt;
   }

   // Azimuth is returned as [-180, 180) from the geodesic inverse, we need a
   // range of [0, 360) NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
   while (azi1 < 0.0)
   {
      azi1 += 360.0;
   }
   // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

   // Compute gate interval
   const size_t gates = p->radialData_->gates();
   const auto   dataMomentInterval =
      static_cast<uint16_t>(p->radialData_->meta_data().dataMomentInterval);
   auto gate = static_cast<size_t>(s12 / dataMomentInterval);

   if (gate >= gates)
   {
      // Coordinate is beyond radar range
      return std::nullopt;
   }

   // Find Radial
   const size_t numRadials = p->radialData_->radials();
   auto         radials    = boost::irange<size_t>(0u, numRadials);

   auto radial = std::find_if( //
      std::execution::par_unseq,
      radials.begin(),
      radials.end(),
      [&](size_t i)
      {
         bool         found      = false;
         const double startAngle = p->radialData_->start_angle(i);
         const double nextAngle =
            p->radialData_->start_angle((i + 1) % numRadials);

         if (startAngle < nextAngle)
         {
            if (startAngle <= azi1 && azi1 < nextAngle)
            {
               found = true;
            }
         }
         else
         {
            // If the bin crosses 0/360 degrees, special handling is needed
            if (startAngle <= azi1 || azi1 < nextAngle)
            {
               found = true;
            }
         }

         return found;
      });

   if (radial == radials.end())
   {
      // No radial was found (not likely to happen without a gap in data)
      return std::nullopt;
   }

   // Compute threshold at which to display an individual bin
   const uint16_t snrThreshold = p->radialData_->meta_data().threshold;
   const uint8_t  level        = p->radialData_->levels(*radial).at(gate);

   if (level < snrThreshold && level != RANGE_FOLDED)
   {
      return std::nullopt;
   }

   return level;
}

boost::asio::thread_pool& DerivedRadialView::thread_pool()
{
   return p->threadPool_;
}

} // namespace scwx::qt::view
