#include <scwx/wsr88d/rpg/storm_tracking_information_message.hpp>
#include <scwx/wsr88d/rpg/text_and_special_symbol_packet.hpp>
#include <scwx/wsr88d/rpg/rpg_types.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/strings.hpp>
#include <scwx/util/time.hpp>

#include <unordered_map>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::storm_tracking_information_message";
static const auto logger_ = util::Logger::Create(logPrefix_);

class StormTrackingInformationMessage::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;

   std::shared_ptr<StiRecord>& GetOrCreateStiRecord(const std::string& stormId);

   void ParseGraphicBlock(
      const std::shared_ptr<const GraphicAlphanumericBlock>& block);
   void ParseTabularBlock(
      const std::shared_ptr<const TabularAlphanumericBlock>& block);

   void ParseStormPositionForecastPage(const std::vector<std::string>& page);
   void ParseStormCellTrackingDataPage(const std::vector<std::string>& page);

   void HandleTextUniformPacket(std::shared_ptr<const Packet> packet,
                                std::vector<std::string>&     stormIds);

   // STORM POSITION/FORECAST
   std::optional<std::uint16_t>                               radarId_ {};
   std::optional<std::chrono::sys_time<std::chrono::seconds>> dateTime_ {};
   std::optional<std::uint16_t>                               numStormCells_ {};

   // STORM CELL TRACKING/FORECAST ADAPTATION DATA
   std::optional<units::angle::degrees<std::uint16_t>> defaultDirection_ {};
   std::optional<units::velocity::meters_per_second<float>> minimumSpeed_ {};
   std::optional<units::velocity::knots<float>>             defaultSpeed_ {};
   std::optional<units::length::kilometers<std::uint16_t>>  allowableError_ {};
   std::optional<std::chrono::minutes>                      maximumTime_ {};
   std::optional<std::chrono::minutes> forecastInterval_ {};
   std::optional<std::uint16_t>        numberOfPastVolumes_ {};
   std::optional<std::uint16_t>        numberOfIntervals_ {};
   std::optional<units::velocity::meters_per_second<float>>
                                       correlationSpeed_ {};
   std::optional<std::chrono::minutes> errorInterval_ {};

   // SCIT REFLECTIVITY MEDIAN FILTER
   std::optional<units::length::kilometers<float>> filterKernelSize_ {};
   std::optional<float>                            filterFraction_ {};
   std::optional<bool>                             reflectivityFiltered_ {};

   std::unordered_map<std::string, std::shared_ptr<StiRecord>> stiRecords_ {};
};

StormTrackingInformationMessage::StormTrackingInformationMessage() :
    p(std::make_unique<Impl>())
{
}
StormTrackingInformationMessage::~StormTrackingInformationMessage() = default;

StormTrackingInformationMessage::StormTrackingInformationMessage(
   StormTrackingInformationMessage&&) noexcept = default;
StormTrackingInformationMessage& StormTrackingInformationMessage::operator=(
   StormTrackingInformationMessage&&) noexcept = default;

std::optional<std::uint16_t> StormTrackingInformationMessage::radar_id() const
{
   return p->radarId_;
}

std::optional<std::chrono::sys_time<std::chrono::seconds>>
StormTrackingInformationMessage::date_time() const
{
   return p->dateTime_;
}

std::optional<std::uint16_t>
StormTrackingInformationMessage::num_storm_cells() const
{
   return p->numStormCells_;
}

std::optional<units::angle::degrees<std::uint16_t>>
StormTrackingInformationMessage::default_direction() const
{
   return p->defaultDirection_;
}

std::optional<units::velocity::meters_per_second<float>>
StormTrackingInformationMessage::minimum_speed() const
{
   return p->minimumSpeed_;
}

std::optional<units::velocity::knots<float>>
StormTrackingInformationMessage::default_speed() const
{
   return p->defaultSpeed_;
}

std::optional<units::length::kilometers<std::uint16_t>>
StormTrackingInformationMessage::allowable_error() const
{
   return p->allowableError_;
}

std::optional<std::chrono::minutes>
StormTrackingInformationMessage::maximum_time() const
{
   return p->maximumTime_;
}

std::optional<std::chrono::minutes>
StormTrackingInformationMessage::forecast_interval() const
{
   return p->forecastInterval_;
}

std::optional<std::uint16_t>
StormTrackingInformationMessage::number_of_past_volumes() const
{
   return p->numberOfPastVolumes_;
}

std::optional<std::uint16_t>
StormTrackingInformationMessage::number_of_intervals() const
{
   return p->numberOfIntervals_;
}

std::optional<units::velocity::meters_per_second<float>>
StormTrackingInformationMessage::correlation_speed() const
{
   return p->correlationSpeed_;
}

std::optional<std::chrono::minutes>
StormTrackingInformationMessage::error_interval() const
{
   return p->errorInterval_;
}

std::optional<units::length::kilometers<float>>
StormTrackingInformationMessage::filter_kernel_size() const
{
   return p->filterKernelSize_;
}

std::optional<float> StormTrackingInformationMessage::filter_fraction() const
{
   return p->filterFraction_;
}

std::optional<bool>
StormTrackingInformationMessage::reflectivity_filtered() const
{
   return p->reflectivityFiltered_;
}

std::shared_ptr<const StormTrackingInformationMessage::StiRecord>
StormTrackingInformationMessage::sti_record(const std::string& stormId) const
{
   std::shared_ptr<const StiRecord> record = nullptr;

   auto it = p->stiRecords_.find(stormId);
   if (it != p->stiRecords_.cend())
   {
      record = it->second;
   }

   return record;
}

std::shared_ptr<StormTrackingInformationMessage::StiRecord>&
StormTrackingInformationMessage::Impl::GetOrCreateStiRecord(
   const std::string& stormId)
{
   auto& record = stiRecords_[stormId];
   if (record == nullptr)
   {
      record = std::make_shared<StiRecord>();
   }
   return record;
}

bool StormTrackingInformationMessage::Parse(std::istream& is)
{
   bool dataValid = GraphicProductMessage::Parse(is);

   std::shared_ptr<GraphicAlphanumericBlock> graphicBlock = nullptr;
   std::shared_ptr<TabularAlphanumericBlock> tabularBlock = nullptr;

   if (dataValid)
   {
      graphicBlock = graphic_block();
      tabularBlock = tabular_block();
   }

   if (graphicBlock != nullptr)
   {
      p->ParseGraphicBlock(graphicBlock);
   }

   if (tabularBlock != nullptr)
   {
      p->ParseTabularBlock(tabularBlock);
   }

   return dataValid;
}

void StormTrackingInformationMessage::Impl::ParseGraphicBlock(
   const std::shared_ptr<const GraphicAlphanumericBlock>& block)
{
   for (auto& page : block->page_list())
   {
      std::vector<std::string> stormIds {};

      for (auto& packet : page)
      {
         switch (packet->packet_code())
         {
         case static_cast<std::uint16_t>(wsr88d::rpg::PacketCode::TextUniform):
            HandleTextUniformPacket(packet, stormIds);
            break;

         default:
            logger_->trace("Ignoring graphic alphanumeric packet type: {}",
                           packet->packet_code());
            break;
         }
      }
   }
}

void StormTrackingInformationMessage::Impl::HandleTextUniformPacket(
   std::shared_ptr<const Packet> packet, std::vector<std::string>& stormIds)
{
   auto textPacket =
      std::dynamic_pointer_cast<const wsr88d::rpg::TextAndSpecialSymbolPacket>(
         packet);

   if (textPacket != nullptr && textPacket->text().size() >= 69)
   {
      auto text = textPacket->text();

      // " STORM ID        D7        H3        K5        N5        U6        E7"
      if (text.starts_with(" STORM ID"))
      {
         static constexpr std::size_t kMaxStormIds = 6;
         static constexpr std::size_t kStartOffset = 17;
         static constexpr std::size_t kStride      = 10;

         stormIds.clear();

         for (std::size_t i = 0, offset = kStartOffset; i < kMaxStormIds;
              ++i, offset += kStride)
         {
            std::string stormId = text.substr(offset, 2);

            if (std::isupper(stormId[0]) && std::isdigit(stormId[1]))
            {
               stormIds.push_back(stormId);
            }
         }
      }

      // " AZ/RAN    242/ 77    45/ 36   180/139   175/126    23/110    25/ 83"
      else if (text.starts_with(" AZ/RAN"))
      {
         static constexpr std::size_t kAzStartOffset  = 11;
         static constexpr std::size_t kRanStartOffset = 15;
         static constexpr std::size_t kStride         = 10;

         for (std::size_t i         = 0,
                          azOffset  = kAzStartOffset,
                          ranOffset = kRanStartOffset;
              i < stormIds.size();
              ++i, azOffset += kStride, ranOffset += kStride)
         {
            auto& record = GetOrCreateStiRecord(stormIds[i]);

            if (!record->currentPosition_.azimuth_.has_value())
            {
               // Current Position: Azimuth (Degrees) (I3)
               auto azimuth = util::TryParseNumeric<std::uint16_t>(
                  text.substr(azOffset, 3));
               if (azimuth.has_value())
               {
                  record->currentPosition_.azimuth_ =
                     units::angle::degrees<std::uint16_t> {azimuth.value()};
               }
            }

            if (!record->currentPosition_.range_.has_value())
            {
               // Current Position: Range (Nautical Miles) (I3)
               auto range = util::TryParseNumeric<std::uint16_t>(
                  text.substr(ranOffset, 3));
               if (range.has_value())
               {
                  record->currentPosition_.range_ =
                     units::length::nautical_miles<std::uint16_t> {
                        range.value()};
               }
            }
         }
      }

      // " FCST MVT  262/ 56   249/ 48   234/ 46   228/ 48   227/ 66   242/ 48"
      else if (text.starts_with(" FCST MVT"))
      {
         static constexpr std::size_t kDirStartOffset   = 11;
         static constexpr std::size_t kSpeedStartOffset = 15;
         static constexpr std::size_t kStride           = 10;

         for (std::size_t i           = 0,
                          dirOffset   = kDirStartOffset,
                          speedOffset = kSpeedStartOffset;
              i < stormIds.size();
              ++i, dirOffset += kStride, speedOffset += kStride)
         {
            auto& record = GetOrCreateStiRecord(stormIds[i]);

            if (!record->direction_.has_value())
            {
               // Movement: Direction (Degrees) (I3)
               auto direction = util::TryParseNumeric<std::uint16_t>(
                  text.substr(dirOffset, 3));
               if (direction.has_value())
               {
                  record->direction_ =
                     units::angle::degrees<std::uint16_t> {direction.value()};
               }
            }

            if (!record->speed_.has_value())
            {
               // Movement: Speed (Knots) (I3)
               auto speed = util::TryParseNumeric<std::uint16_t>(
                  text.substr(speedOffset, 3));
               if (speed.has_value())
               {
                  record->speed_ =
                     units::velocity::knots<std::uint16_t> {speed.value()};
               }
            }
         }
      }

      // " ERR/MEAN  4.5/ 2.9  0.8/ 1.7  1.4/ 1.4  1.3/ 1.3  1.4/ 1.7  1.2/ 0.8"
      else if (text.starts_with(" ERR/MEAN"))
      {
         static constexpr std::size_t kErrStartOffset  = 10;
         static constexpr std::size_t kMeanStartOffset = 15;
         static constexpr std::size_t kStride          = 10;

         for (std::size_t i          = 0,
                          errOffset  = kErrStartOffset,
                          meanOffset = kMeanStartOffset;
              i < stormIds.size();
              ++i, errOffset += kStride, meanOffset += kStride)
         {
            auto& record = GetOrCreateStiRecord(stormIds[i]);

            if (!record->forecastError_.has_value())
            {
               // Forecast Error (Nautical Miles) (F4.1)
               auto forecastError =
                  util::TryParseNumeric<float>(text.substr(errOffset, 4));
               if (forecastError.has_value())
               {
                  record->forecastError_ =
                     units::length::nautical_miles<float> {
                        forecastError.value()};
               }
            }

            if (!record->meanError_.has_value())
            {
               // Mean Error (Nautical Miles) (F4.1)
               auto meanError =
                  util::TryParseNumeric<float>(text.substr(meanOffset, 4));
               if (meanError.has_value())
               {
                  record->meanError_ =
                     units::length::nautical_miles<float> {meanError.value()};
               }
            }
         }
      }

      // " DBZM HGT   55  7.5   56  4.2   48 20.6   51 17.4   51 14.0   54  8.9"
      else if (text.starts_with(" DBZM HGT"))
      {
         static constexpr std::size_t kDbzmStartOffset = 12;
         static constexpr std::size_t kHgtStartOffset  = 15;
         static constexpr std::size_t kStride          = 10;

         for (std::size_t i          = 0,
                          dbzmOffset = kDbzmStartOffset,
                          hgtOffset  = kHgtStartOffset;
              i < stormIds.size();
              ++i, dbzmOffset += kStride, hgtOffset += kStride)
         {
            auto& record = GetOrCreateStiRecord(stormIds[i]);

            // Maximum dBZ (I2)
            record->maxDbz_ =
               util::TryParseNumeric<std::uint16_t>(text.substr(dbzmOffset, 2));

            // Maximum dBZ Height (Feet) (F4.1)
            auto height =
               util::TryParseNumeric<float>(text.substr(hgtOffset, 4));
            if (height.has_value())
            {
               record->maxDbzHeight_ = units::length::feet<std::uint32_t> {
                  static_cast<std::uint32_t>(height.value() * 1000.0f)};
            }
         }
      }
   }
   else
   {
      logger_->warn("Invalid Text Uniform Packet");
   }
}

void StormTrackingInformationMessage::Impl::ParseTabularBlock(
   const std::shared_ptr<const TabularAlphanumericBlock>& block)
{
   static const std::string kStormPositionForecast_ = "STORM POSITION/FORECAST";
   static const std::string kStormCellTrackingData_ =
      "STORM CELL TRACKING/FORECAST ADAPTATION DATA";

   for (auto& page : block->page_list())
   {
      if (page.empty())
      {
         logger_->warn("Unexpected empty page");
         continue;
      }

      if (page[0].find(kStormPositionForecast_) != std::string::npos)
      {
         ParseStormPositionForecastPage(page);
      }
      else if (page[0].find(kStormCellTrackingData_) != std::string::npos)
      {
         ParseStormCellTrackingDataPage(page);
      }
   }
}

void StormTrackingInformationMessage::Impl::ParseStormPositionForecastPage(
   const std::vector<std::string>& page)
{
   for (std::size_t i = 1; i < page.size(); ++i)
   {
      const std::string& line = page[i];

      // clang-format off
      // "     RADAR ID 308  DATE/TIME 12:11:21/02:15:38   NUMBER OF STORM CELLS  34"
      // clang-format on
      if (i == 1 && line.size() >= 74)
      {
         if (!radarId_.has_value())
         {
            // Radar ID (I3)
            radarId_ = util::TryParseNumeric<std::uint16_t>(line.substr(14, 3));
         }
         if (!dateTime_.has_value())
         {
            static const std::string kDateTimeFormat_ {"%m:%d:%y/%H:%M:%S"};

            dateTime_ = util::TryParseDateTime<std::chrono::seconds>(
               kDateTimeFormat_, line.substr(29, 17));
         }
         if (!numStormCells_.has_value())
         {
            // Number of Storm Cells (I3)
            numStormCells_ =
               util::TryParseNumeric<std::uint16_t>(line.substr(71, 3));
         }
      }
      // clang-format off
      // "  V6     183/147   234/ 63     178/137   172/129   166/122   159/117    0.7/ 0.7"
      // clang-format on
      else if (i >= 7 && line.size() >= 80)
      {
         std::string stormId = line.substr(2, 2);

         if (std::isupper(stormId[0]) && std::isdigit(stormId[1]))
         {
            auto& record = GetOrCreateStiRecord(stormId);

            if (!record->currentPosition_.azimuth_.has_value())
            {
               // Current Position: Azimuth (Degrees) (I3)
               auto azimuth =
                  util::TryParseNumeric<std::uint16_t>(line.substr(9, 3));
               if (azimuth.has_value())
               {
                  record->currentPosition_.azimuth_ =
                     units::angle::degrees<std::uint16_t> {azimuth.value()};
               }
            }
            if (!record->currentPosition_.range_.has_value())
            {
               // Current Position: Range (Nautical Miles) (I3)
               auto range =
                  util::TryParseNumeric<std::uint16_t>(line.substr(13, 3));
               if (range.has_value())
               {
                  record->currentPosition_.range_ =
                     units::length::nautical_miles<std::uint16_t> {
                        range.value()};
               }
            }
            if (!record->direction_.has_value())
            {
               // Movement: Direction (Degrees) (I3)
               auto direction =
                  util::TryParseNumeric<std::uint16_t>(line.substr(19, 3));
               if (direction.has_value())
               {
                  record->direction_ =
                     units::angle::degrees<std::uint16_t> {direction.value()};
               }
            }
            if (!record->speed_.has_value())
            {
               // Movement: Speed (Knots) (I3)
               auto speed =
                  util::TryParseNumeric<std::uint16_t>(line.substr(23, 3));
               if (speed.has_value())
               {
                  record->speed_ =
                     units::velocity::knots<std::uint16_t> {speed.value()};
               }
            }
            for (std::size_t j = 0; j < record->forecastPosition_.size(); ++j)
            {
               const std::size_t positionOffset = j * 10;

               if (!record->forecastPosition_[j].azimuth_.has_value())
               {
                  // Forecast Position: Azimuth (Degrees) (I3)
                  std::size_t offset = 31 + positionOffset;

                  auto azimuth = util::TryParseNumeric<std::uint16_t>(
                     line.substr(offset, 3));
                  if (azimuth.has_value())
                  {
                     record->forecastPosition_[j].azimuth_ =
                        units::angle::degrees<std::uint16_t> {azimuth.value()};
                  }
               }
               if (!record->forecastPosition_[j].range_.has_value())
               {
                  // Forecast Position: Range (Nautical Miles) (I3)
                  std::size_t offset = 35 + positionOffset;

                  auto range = util::TryParseNumeric<std::uint16_t>(
                     line.substr(offset, 3));
                  if (range.has_value())
                  {
                     record->forecastPosition_[j].range_ =
                        units::length::nautical_miles<std::uint16_t> {
                           range.value()};
                  }
               }
            }
            if (!record->forecastError_.has_value())
            {
               // Forecast Error (Nautical Miles) (F4.1)
               auto forecastError =
                  util::TryParseNumeric<float>(line.substr(71, 4));
               if (forecastError.has_value())
               {
                  record->forecastError_ =
                     units::length::nautical_miles<float> {
                        forecastError.value()};
               }
            }
            if (!record->meanError_.has_value())
            {
               // Mean Error (Nautical Miles) (F4.1)
               auto meanError =
                  util::TryParseNumeric<float>(line.substr(76, 4));
               if (meanError.has_value())
               {
                  record->meanError_ =
                     units::length::nautical_miles<float> {meanError.value()};
               }
            }
         }
      }
   }
}

void StormTrackingInformationMessage::Impl::ParseStormCellTrackingDataPage(
   const std::vector<std::string>& page)
{
   for (std::size_t i = 1; i < page.size(); ++i)
   {
      const std::string& line = page[i];

      // clang-format off
      // "    260   (DEG) DEFAULT (DIRECTION)      2.5   (M/S) THRESH (MINIMUM SPEED)"
      // clang-format on
      if (i == 2 && line.size() >= 75)
      {
         // Default Direction (Degrees) (I3)
         auto direction =
            util::TryParseNumeric<std::uint16_t>(line.substr(4, 3));
         if (direction.has_value())
         {
            defaultDirection_ =
               units::angle::degrees<std::uint16_t> {direction.value()};
         }

         // Minimum Speed (Threshold) (m/s) (F4.1)
         auto threshold = util::TryParseNumeric<float>(line.substr(40, 4));
         if (threshold.has_value())
         {
            minimumSpeed_ =
               units::velocity::meters_per_second<float> {threshold.value()};
         }
      }

      // "   36.0   (KTS) DEFAULT (SPEED)           20    (KM) ALLOWABLE ERROR"
      if (i == 3 && line.size() >= 68)
      {
         // Default Speed (Knots) (F4.1)
         auto speed = util::TryParseNumeric<float>(line.substr(3, 4));
         if (speed.has_value())
         {
            defaultSpeed_ = units::velocity::knots<float> {speed.value()};
         }

         // Allowable Error (Kilometers) (I2)
         auto error = util::TryParseNumeric<std::uint16_t>(line.substr(42, 2));
         if (error.has_value())
         {
            allowableError_ =
               units::length::kilometers<std::uint16_t> {error.value()};
         }
      }

      // clang-format off
      // "     20   (MIN) TIME (MAXIMUM)            15   (MIN) FORECAST INTERVAL"
      // clang-format on
      if (i == 4 && line.size() >= 70)
      {
         // Maximum Time (Minutes) (I5)
         auto time = util::TryParseNumeric<std::uint32_t>(line.substr(2, 5));
         if (time.has_value())
         {
            maximumTime_ = std::chrono::minutes {time.value()};
         }

         // Forecast Interval (Minutes) (I2)
         auto interval =
            util::TryParseNumeric<std::uint16_t>(line.substr(42, 2));
         if (interval.has_value())
         {
            forecastInterval_ = std::chrono::minutes {interval.value()};
         }
      }

      // clang-format off
      // "     10         NUMBER OF PAST VOLUMES     4         NUMBER OF INTERVALS"
      // clang-format on
      if (i == 5 && line.size() >= 72)
      {
         // Number of Past Volumes (I2)
         numberOfPastVolumes_ =
            util::TryParseNumeric<std::uint16_t>(line.substr(5, 2));

         // Number of Intervals (I2)
         numberOfIntervals_ =
            util::TryParseNumeric<std::uint16_t>(line.substr(42, 2));
      }

      // "   30.0   (M/S) CORRELATION SPEED         15   (MIN) ERROR INTERVAL"
      if (i == 6 && line.size() >= 67)
      {
         // Correlation Speed (m/s) (F4.1)
         auto speed = util::TryParseNumeric<float>(line.substr(3, 4));
         if (speed.has_value())
         {
            correlationSpeed_ =
               units::velocity::meters_per_second<float> {speed.value()};
         }

         // Error Interval (Minutes) (I2)
         auto interval =
            util::TryParseNumeric<std::uint16_t>(line.substr(42, 2));
         if (interval.has_value())
         {
            errorInterval_ = std::chrono::minutes {interval.value()};
         }
      }

      // clang-format off
      // "    7.0   (KM)  FILTER KERNEL SIZE       0.5         THRESH (FILTER FRACTION)"
      // clang-format on
      if (i == 11 && line.size() >= 77)
      {
         // Filter Kernel Size (Kilometers) (F4.1)
         auto kernelSize = util::TryParseNumeric<float>(line.substr(3, 4));
         if (kernelSize.has_value())
         {
            filterKernelSize_ =
               units::length::kilometers<float> {kernelSize.value()};
         }

         // Minimum Speed (Threshold) (m/s) (F4.1)
         filterFraction_ = util::TryParseNumeric<float>(line.substr(40, 4));
      }

      // "    Yes         REFLECTIVITY FILTERED"
      if (i == 12 && line.size() >= 37)
      {
         if (line.substr(4, 3) == "Yes")
         {
            reflectivityFiltered_ = true;
         }
         else if (line.substr(5, 2) == "No")
         {
            reflectivityFiltered_ = false;
         }
      }
   }
}

std::shared_ptr<StormTrackingInformationMessage>
StormTrackingInformationMessage::Create(Level3MessageHeader&& header,
                                        std::istream&         is)
{
   std::shared_ptr<StormTrackingInformationMessage> message =
      std::make_shared<StormTrackingInformationMessage>();
   message->set_header(std::move(header));

   if (!message->Parse(is))
   {
      message.reset();
   }

   return message;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
