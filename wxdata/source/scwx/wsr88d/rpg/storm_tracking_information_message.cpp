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
         // TODO: STI Record
         std::string stormId = line.substr(2, 2);
         (void) (stormId);
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
