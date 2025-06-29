#include <scwx/deriver/l2_kdp_deriver.hpp>

#include <scwx/deriver/data/derived_radial_data.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>

namespace scwx::deriver
{

static const std::string logPrefix_ = "scwx::deriver::l2_kdp_deriver";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class KdpDeriver::Impl
{
public:
   explicit Impl() = default;

   // TODO this should be moved into a util file
   static units::degrees<float> NormalizeAngle(units::degrees<float> angle);
};

units::degrees<float>
KdpDeriver::Impl::NormalizeAngle(units::degrees<float> angle)
{
   constexpr auto angleLimit = units::degrees<float> {180.0f};
   constexpr auto fullAngle  = units::degrees<float> {360.0f};

   // Normalize angle to [-180, 180)
   while (angle < -angleLimit)
   {
      angle += fullAngle;
   }
   while (angle >= angleLimit)
   {
      angle -= fullAngle;
   }

   return angle;
}

KdpDeriver::KdpDeriver() : p {std::make_unique<Impl>()} {}
KdpDeriver::~KdpDeriver() = default;

std::shared_ptr<data::DerivedData>
KdpDeriver::GetOutput(const std::string& product)
{
   // Find which products we need for this elevation
   const auto& derivedProductIt = deriveable_products().find(product);
   if (derivedProductIt == deriveable_products().cend())
   {
      return nullptr;
   }
   const auto& derivedProduct = derivedProductIt->second;

   const auto& [dataBlockType, elevation] = derivedProduct.level2Products_[0];

   const std::shared_ptr<wsr88d::rda::ElevationScan> radarData =
      GetLevel2Input(dataBlockType, elevation);
   if (radarData == nullptr)
   {
      return nullptr;
   }

   const size_t radials = radarData->crbegin()->first + 1;

   auto& radarData0  = (*radarData)[0];
   auto  momentData0 = radarData0->moment_data_block(dataBlockType);

   if (momentData0 == nullptr)
   {
      // TODO
      logger_->warn("No moment data");
      return nullptr;
   }

   const size_t gates       = momentData0->number_of_data_moment_gates();
   const float  inputScale  = momentData0->scale();
   const float  inputOffset = momentData0->offset();

   // These are based on NWS's values
   constexpr float minValue = -2;
   constexpr float maxValue = 10;

   const float outputScale = 254 / (maxValue - minValue);

   const size_t extraGates =
      momentData0->data_moment_range_raw() /
      momentData0->data_moment_range_sample_interval_raw();
   auto output =
      std::make_shared<data::DerivedRadialData>(radials, extraGates + gates);

   for (size_t radial = 0; radial < radials; radial++)
   {
      units::degrees<float> angle {};
      units::degrees<float> deltaAngle {};

      auto radialData = radarData->find(radial);
      auto prevRadial1 =
         radarData->find((radial >= 1) ? radial - 1 : radials - (1 - radial));
      auto prevRadial2 =
         radarData->find((radial >= 2) ? radial - 2 : radials - (2 - radial));

      if (radialData != radarData->cend() && prevRadial1 != radarData->cend())
      {
         const units::degrees<float> currentAngle =
            radialData->second->azimuth_angle();
         const units::degrees<float> prevAngle =
            prevRadial1->second->azimuth_angle();
         deltaAngle = p->NormalizeAngle(currentAngle - prevAngle);

         // Delta scale is half the delta angle to reach the end of the bin
         constexpr float deltaScale = 0.5f;

         angle = currentAngle - deltaAngle * deltaScale;
      }
      else if (radialData != radarData->cend())
      {
         const units::degrees<float> currentAngle =
            radialData->second->azimuth_angle();

         // Assume a half degree delta if there aren't enough angles to
         // determine a delta angle
         // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
         deltaAngle = units::degrees<float>(0.5f);

         // Delta scale is half the delta angle to reach the edge of the bin
         constexpr float deltaScale = 0.5f;

         angle = currentAngle - deltaAngle * deltaScale;
      }
      else if (prevRadial1 != radarData->cend() &&
               prevRadial2 != radarData->cend())
      {
         const units::degrees<float> prevAngle1 =
            prevRadial1->second->azimuth_angle();
         const units::degrees<float> prevAngle2 =
            prevRadial2->second->azimuth_angle();

         // Calculate delta angle
         deltaAngle = p->NormalizeAngle(prevAngle1 - prevAngle2);

         // Delta scale is half the delta angle to reach the edge of the bin
         constexpr float deltaScale = 0.5f;

         angle = prevAngle1 + deltaAngle * deltaScale;
      }
      else if (prevRadial1 != radarData->cend())
      {
         const units::degrees<float> prevAngle1 =
            prevRadial1->second->azimuth_angle();

         // Assume a half degree delta if there aren't enough angles
         // to determine a delta angle
         // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
         deltaAngle = units::degrees<float>(0.5f);

         // Delta scale is half the delta angle to reach the edge of the bin
         const float deltaScale = 0.5f;

         angle = prevAngle1 + deltaAngle * deltaScale;
      }
      else
      {
         // Not enough angles present to determine an angle
         // TODO
         return nullptr;
      }

      output->SetRadial(radial, angle.value(), deltaAngle.value());

      const std::shared_ptr<wsr88d::rda::GenericRadarData::MomentDataBlock>
         momentData = radialData->second->moment_data_block(dataBlockType);
      if (momentData0->data_word_size() != momentData->data_word_size())
      {
         logger_->warn("Radial {} has different word size", radial);
         continue;
      }

      // TODO will this change?
      const float dataMomentIntervalKm =
         units::length::kilometers<float>(
            momentData->data_moment_range_sample_interval())
            .value();

      const uint8_t*  dataMomentsArray8  = nullptr;
      const uint16_t* dataMomentsArray16 = nullptr;

      // NOLINTNEXTLINE Choose 8 or 16 bit words
      if (momentData->data_word_size() == 8)
      {
         dataMomentsArray8 =
            reinterpret_cast<const std::uint8_t*>(momentData->data_moments());
      }
      else
      {
         dataMomentsArray16 =
            reinterpret_cast<const std::uint16_t*>(momentData->data_moments());
      }

      constexpr float belowThreshold = std::numeric_limits<float>::infinity();
      constexpr float rangeFolded    = -std::numeric_limits<float>::infinity();

      auto sins = std::vector<float>(gates, 0);
      auto coss = std::vector<float>(gates, 0);

      for (size_t gate = 0; gate < gates; gate++)
      {
         // There will be gates items, so this is safe
         // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
         const uint16_t level = dataMomentsArray8 != nullptr ?
                                   dataMomentsArray8[gate] :
                                   dataMomentsArray16[gate];
         // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

         if (level == 0)
         {
            sins[gate] = belowThreshold;
            coss[gate] = belowThreshold;
            continue;
         }
         else if (level == 1)
         {
            sins[gate] = rangeFolded;
            coss[gate] = rangeFolded;
            continue;
         }

         const units::radians<float> value = units::degrees<float>(
            (static_cast<float>(level) - inputOffset) / inputScale);
         sins[gate] = std::sin(value.value());
         coss[gate] = std::cos(value.value());
      }

      auto& radialLevels = output->levels(radial);

      for (size_t absGate = 0; absGate < extraGates + gates; absGate++)
      {
         if (absGate < extraGates)
         {
            radialLevels[absGate] = 0;
            continue;
         }
         const size_t gate = absGate - extraGates;
         const float  sin  = sins[gate];

         if (sin == belowThreshold)
         {
            radialLevels[absGate] = 0;
            continue;
         }
         else if (sin == rangeFolded)
         {
            radialLevels[absGate] = 1;
            continue;
         }
         constexpr size_t NUM_SMOOTHING_GATES = 10;

         size_t count  = 0;
         float  sinSum = 0;
         float  cosSum = 0;
         for (size_t sumGate = gate - std::min(NUM_SMOOTHING_GATES, gate);
              sumGate < gate;
              sumGate++)
         {
            const float sin = sins[sumGate];
            const float cos = coss[sumGate];
            if (sin == belowThreshold || sin == rangeFolded)
            {
               continue;
            }

            count += 1;
            sinSum += sin;
            cosSum += cos;
         }
         if (count == 0)
         {
            // Below threshold
            radialLevels[absGate] = 0;
            continue;
         }
         float beforeMean = std::atan2(sinSum / static_cast<float>(count),
                                       cosSum / static_cast<float>(count));

         sinSum = 0;
         cosSum = 0;
         count  = 0;
         for (size_t sumGate = gate;
              sumGate < std::min(gate + NUM_SMOOTHING_GATES, gates);
              sumGate++)
         {
            const float sin = sins[sumGate];
            const float cos = coss[sumGate];
            if (sin == belowThreshold || sin == rangeFolded)
            {
               continue;
            }

            count += 1;
            sinSum += sin;
            cosSum += cos;
         }
         if (count == 0)
         {
            // Below threshold
            radialLevels[absGate] = 0;
            continue;
         }
         float afterMean = std::atan2(sinSum / static_cast<float>(count),
                                      cosSum / static_cast<float>(count));

         // Convert Back to degrees
         beforeMean =
            units::degrees<float>(units::radians<float>(beforeMean)).value();
         afterMean =
            units::degrees<float>(units::radians<float>(afterMean)).value();

         const float difference =
            p->NormalizeAngle(units::degrees<float>(afterMean - beforeMean))
               .value();

         // Output data in degrees per km
         const float value =
            difference / (dataMomentIntervalKm * NUM_SMOOTHING_GATES * 2);

         radialLevels[absGate] =
            static_cast<uint8_t>((value - minValue) * outputScale);
      }
   }

   output->meta_data().hasElevation = true;
   output->meta_data().elevation    = units::angle::degrees<float>(elevation);
   output->meta_data().scale        = 1 / outputScale;
   output->meta_data().offset       = minValue;
   output->meta_data().range =
      momentData0->data_moment_range() +
      momentData0->data_moment_range_sample_interval() *
         // TODO  Why is it - 0.5?
         (static_cast<float>(gates) - 0.5f);
   output->meta_data().dataMomentInterval = units::length::meters<float>(
      static_cast<float>(momentData0->data_moment_range_sample_interval_raw()));
   output->meta_data().sweepTime = scwx::util::TimePoint(
      radarData0->modified_julian_date(), radarData0->collection_time());
   output->meta_data().vcp       = radarData0->volume_coverage_pattern_number();
   output->meta_data().threshold = 2;
   // NOLINTNEXTLINE This is a 256 level
   output->meta_data().numberOfLevels = 256;

   return output;
}

const std::unordered_map<std::string, DerivedProductInfo>&
KdpDeriver::deriveable_products()
{
   const static std::unordered_map<std::string, DerivedProductInfo>
      derivableProducts_ = {
         // TODO
         {"KDP--0.2",
          {"KDP--0.2", {}, {{wsr88d::rda::DataBlockType::MomentPhi, -0.2}}}},
         {"KDP-0.0",
          {"KDP-0.0", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 0.0}}}},
         {"KDP-0.2",
          {"KDP-0.2", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 0.2}}}},
         {"KDP-0.3",
          {"KDP-0.3", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 0.3}}}},
         {"KDP-0.4",
          {"KDP-0.4", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 0.4}}}},
         {"KDP-0.5",
          {"KDP-0.5", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 0.5}}}},
         {"KDP-0.9",
          {"KDP-0.9", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 0.9}}}},
         {"KDP-1.3",
          {"KDP-1.3", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 1.3}}}},
         {"KDP-1.5",
          {"KDP-1.5", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 1.5}}}},
         {"KDP-1.8",
          {"KDP-1.8", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 1.8}}}},
         {"KDP-2.4",
          {"KDP-2.4", {}, {{wsr88d::rda::DataBlockType::MomentPhi, 2.4}}}},
      };
   return derivableProducts_;
}

} // namespace scwx::deriver
