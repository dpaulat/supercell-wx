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
            radarId_ =
               util::TryParseUnsignedLong<std::uint16_t>(line.substr(14, 3));
         }
         if (dateTime_ == std::nullopt)
         {
            static const std::string kDateTimeFormat_ {"%m:%d:%y/%H:%M:%S"};

            dateTime_ = util::TryParseDateTime<std::chrono::seconds>(
               kDateTimeFormat_, line.substr(29, 17));
         }
         if (numStormCells_ == std::nullopt)
         {
            numStormCells_ =
               util::TryParseUnsignedLong<std::uint16_t>(line.substr(71, 3));
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
               // Current Position: Azimuth (Degrees)
               auto azimuth =
                  util::TryParseUnsignedLong<std::uint16_t>(line.substr(9, 3));
               if (azimuth.has_value())
               {
                  record.currentPosition_.azimuth_ =
                     units::angle::degrees<std::uint16_t> {azimuth.value()};
               }
            }
            if (record.currentPosition_.range_ == std::nullopt)
            {
               // Current Position: Range (Nautical Miles)
               auto range =
                  util::TryParseUnsignedLong<std::uint16_t>(line.substr(13, 3));
               if (range.has_value())
               {
                  record.currentPosition_.range_ =
                     units::length::nautical_miles<std::uint16_t> {
                        range.value()};
               }
            }
            if (record.direction_ == std::nullopt)
            {
               // Movement: Direction (Degrees)
               auto direction =
                  util::TryParseUnsignedLong<std::uint16_t>(line.substr(19, 3));
               if (direction.has_value())
               {
                  record.direction_ =
                     units::angle::degrees<std::uint16_t> {direction.value()};
               }
            }
            if (record.speed_ == std::nullopt)
            {
               // Movement: Speed (Knots)
               auto speed =
                  util::TryParseUnsignedLong<std::uint16_t>(line.substr(23, 3));
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
                  // Forecast Position: Azimuth (Degrees)
                  std::size_t offset = 31 + positionOffset;

                  auto azimuth = util::TryParseUnsignedLong<std::uint16_t>(
                     line.substr(offset, 3));
                  if (azimuth.has_value())
                  {
                     record.forecastPosition_[j].azimuth_ =
                        units::angle::degrees<std::uint16_t> {azimuth.value()};
                  }
               }
               if (record.forecastPosition_[j].range_ == std::nullopt)
               {
                  // Forecast Position: Range (Nautical Miles)
                  std::size_t offset = 35 + positionOffset;

                  auto range = util::TryParseUnsignedLong<std::uint16_t>(
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
               // Forecast Error (Nautical Miles)
               auto forecastError = util::TryParseFloat(line.substr(71, 4));
               if (forecastError.has_value())
               {
                  record.forecastError_ = units::length::nautical_miles<float> {
                     forecastError.value()};
               }
            }
            if (record.meanError_ == std::nullopt)
            {
               // Mean Error (Nautical Miles)
               auto meanError = util::TryParseFloat(line.substr(76, 4));
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
   // TODO
   (void) (page);
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
