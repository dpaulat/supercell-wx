#include <scwx/wsr88d/rpg/storm_tracking_information_message.hpp>
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

   void ParseGraphicBlock(
      const std::shared_ptr<const GraphicAlphanumericBlock>& block);
   void ParseTabularBlock(
      const std::shared_ptr<const TabularAlphanumericBlock>& block);

   void ParseStormPositionForecastPage(const std::vector<std::string>& page);
   void ParseStormCellTrackingDataPage(const std::vector<std::string>& page);

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

   std::unordered_map<std::string, StiRecord> stiRecords_ {};
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
   // TODO
   (void) (block);
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
         if (radarId_ == std::nullopt)
         {
            // Radar ID (I3)
            radarId_ = util::TryParseNumeric<std::uint16_t>(line.substr(14, 3));
         }
         if (dateTime_ == std::nullopt)
         {
            static const std::string kDateTimeFormat_ {"%m:%d:%y/%H:%M:%S"};

            dateTime_ = util::TryParseDateTime<std::chrono::seconds>(
               kDateTimeFormat_, line.substr(29, 17));
         }
         if (numStormCells_ == std::nullopt)
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
            auto& record = stiRecords_[stormId];

            if (record.currentPosition_.azimuth_ == std::nullopt)
            {
               // Current Position: Azimuth (Degrees) (I3)
               auto azimuth =
                  util::TryParseNumeric<std::uint16_t>(line.substr(9, 3));
               if (azimuth.has_value())
               {
                  record.currentPosition_.azimuth_ =
                     units::angle::degrees<std::uint16_t> {azimuth.value()};
               }
            }
            if (record.currentPosition_.range_ == std::nullopt)
            {
               // Current Position: Range (Nautical Miles) (I3)
               auto range =
                  util::TryParseNumeric<std::uint16_t>(line.substr(13, 3));
               if (range.has_value())
               {
                  record.currentPosition_.range_ =
                     units::length::nautical_miles<std::uint16_t> {
                        range.value()};
               }
            }
            if (record.direction_ == std::nullopt)
            {
               // Movement: Direction (Degrees) (I3)
               auto direction =
                  util::TryParseNumeric<std::uint16_t>(line.substr(19, 3));
               if (direction.has_value())
               {
                  record.direction_ =
                     units::angle::degrees<std::uint16_t> {direction.value()};
               }
            }
            if (record.speed_ == std::nullopt)
            {
               // Movement: Speed (Knots) (I3)
               auto speed =
                  util::TryParseNumeric<std::uint16_t>(line.substr(23, 3));
               if (speed.has_value())
               {
                  record.speed_ =
                     units::velocity::knots<std::uint16_t> {speed.value()};
               }
            }
            for (std::size_t j = 0; j < record.forecastPosition_.size(); ++j)
            {
               const std::size_t positionOffset = j * 10;

               if (record.forecastPosition_[j].azimuth_ == std::nullopt)
               {
                  // Forecast Position: Azimuth (Degrees) (I3)
                  std::size_t offset = 31 + positionOffset;

                  auto azimuth = util::TryParseNumeric<std::uint16_t>(
                     line.substr(offset, 3));
                  if (azimuth.has_value())
                  {
                     record.forecastPosition_[j].azimuth_ =
                        units::angle::degrees<std::uint16_t> {azimuth.value()};
                  }
               }
               if (record.forecastPosition_[j].range_ == std::nullopt)
               {
                  // Forecast Position: Range (Nautical Miles) (I3)
                  std::size_t offset = 35 + positionOffset;

                  auto range = util::TryParseNumeric<std::uint16_t>(
                     line.substr(offset, 3));
                  if (range.has_value())
                  {
                     record.forecastPosition_[j].range_ =
                        units::length::nautical_miles<std::uint16_t> {
                           range.value()};
                  }
               }
            }
            if (record.forecastError_ == std::nullopt)
            {
               // Forecast Error (Nautical Miles) (F4.1)
               auto forecastError =
                  util::TryParseNumeric<float>(line.substr(71, 4));
               if (forecastError.has_value())
               {
                  record.forecastError_ = units::length::nautical_miles<float> {
                     forecastError.value()};
               }
            }
            if (record.meanError_ == std::nullopt)
            {
               // Mean Error (Nautical Miles) (F4.1)
               auto meanError =
                  util::TryParseNumeric<float>(line.substr(76, 4));
               if (meanError.has_value())
               {
                  record.meanError_ =
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
      // clang-format off
      // "   36.0   (KTS) DEFAULT (SPEED)           20    (KM) ALLOWABLE ERROR"
      // clang-format on
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
      // clang-format off
      // "   30.0   (M/S) CORRELATION SPEED         15   (MIN) ERROR INTERVAL"
      // clang-format on
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
      // clang-format off
      // "    Yes         REFLECTIVITY FILTERED"
      // clang-format on
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
