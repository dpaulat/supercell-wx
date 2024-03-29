#include <scwx/qt/view/level3_product_view.hpp>
#include <scwx/qt/settings/general_settings.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

#include <limits>
#include <unordered_set>

#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>
#include <fmt/format.h>

#if !defined(_MSC_VER)
#   include <date/date.h>
#endif

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ = "scwx::qt::view::level3_product_view";
static const auto        logger_    = util::Logger::Create(logPrefix_);

static constexpr uint16_t RANGE_FOLDED = 1u;

class Level3ProductView::Impl
{
public:
   explicit Impl(const std::string& product) :
       product_ {product},
       graphicMessage_ {nullptr},
       colorTable_ {},
       colorTableLut_ {},
       colorTableMin_ {2},
       colorTableMax_ {254},
       savedColorTable_ {nullptr},
       savedScale_ {0.0f},
       savedOffset_ {0.0f}
   {
   }
   ~Impl() = default;

   std::string product_;

   std::shared_ptr<wsr88d::rpg::GraphicProductMessage> graphicMessage_;

   std::shared_ptr<common::ColorTable>    colorTable_;
   std::vector<boost::gil::rgba8_pixel_t> colorTableLut_;
   uint16_t                               colorTableMin_;
   uint16_t                               colorTableMax_;

   std::shared_ptr<common::ColorTable> savedColorTable_;
   float                               savedScale_ {1.0f};
   float                               savedOffset_ {0.0f};
   std::uint16_t                       savedLogStart_ {20u};
   float                               savedLogScale_ {1.0f};
   float                               savedLogOffset_ {0.0f};
};

Level3ProductView::Level3ProductView(
   const std::string&                            product,
   std::shared_ptr<manager::RadarProductManager> radarProductManager) :
    RadarProductView(radarProductManager), p(std::make_unique<Impl>(product))
{
   ConnectRadarProductManager();
}
Level3ProductView::~Level3ProductView() = default;

void Level3ProductView::ConnectRadarProductManager()
{
   connect(radar_product_manager().get(),
           &manager::RadarProductManager::DataReloaded,
           this,
           [this](std::shared_ptr<types::RadarProductRecord> record)
           {
              if (record->radar_product_group() ==
                     common::RadarProductGroup::Level3 &&
                  record->radar_product() == p->product_ &&
                  record->time() == selected_time())
              {
                 // If the data associated with the currently selected time is
                 // reloaded, update the view
                 Update();
              }
           });
}

void Level3ProductView::DisconnectRadarProductManager()
{
   disconnect(radar_product_manager().get(),
              &manager::RadarProductManager::DataReloaded,
              this,
              nullptr);
}

std::shared_ptr<common::ColorTable> Level3ProductView::color_table() const
{
   return p->colorTable_;
}

const std::vector<boost::gil::rgba8_pixel_t>&
Level3ProductView::color_table_lut() const
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

uint16_t Level3ProductView::color_table_min() const
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

uint16_t Level3ProductView::color_table_max() const
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

std::shared_ptr<wsr88d::rpg::GraphicProductMessage>
Level3ProductView::graphic_product_message() const
{
   return p->graphicMessage_;
}

void Level3ProductView::set_graphic_product_message(
   std::shared_ptr<wsr88d::rpg::GraphicProductMessage> gpm)
{
   p->graphicMessage_ = gpm;
}

common::RadarProductGroup Level3ProductView::GetRadarProductGroup() const
{
   return common::RadarProductGroup::Level3;
}

std::string Level3ProductView::GetRadarProductName() const
{
   return p->product_;
}

void Level3ProductView::SelectProduct(const std::string& productName)
{
   p->product_ = productName;
}

std::vector<std::pair<std::string, std::string>>
Level3ProductView::GetDescriptionFields() const
{
   std::vector<std::pair<std::string, std::string>> description {};

   if (p->graphicMessage_ != nullptr)
   {
      util::ClockFormat clockFormat = util::GetClockFormat(
         settings::GeneralSettings::Instance().clock_format().GetValue());

      const scwx::util::time_zone* currentZone;

#if defined(_MSC_VER)
      currentZone = std::chrono::current_zone();
#else
      currentZone = date::current_zone();
#endif

      auto descriptionBlock = p->graphicMessage_->description_block();

      if (descriptionBlock != nullptr)
      {
         auto volumeTime = scwx::util::TimePoint(
            descriptionBlock->volume_scan_date(),
            descriptionBlock->volume_scan_start_time() * 1000);
         auto productTime = scwx::util::TimePoint(
            descriptionBlock->generation_date_of_product(),
            descriptionBlock->generation_time_of_product() * 1000);

         description.emplace_back(
            "Volume Time",
            scwx::util::TimeString(
               volumeTime, clockFormat, currentZone, false));
         description.emplace_back(
            "Product Time",
            scwx::util::TimeString(
               productTime, clockFormat, currentZone, false));

         description.emplace_back(
            "Sequence Number",
            fmt::format("{}", descriptionBlock->sequence_number()));
         description.emplace_back(
            "Volume Scan",
            fmt::format("{}", descriptionBlock->volume_scan_number()));

         if (descriptionBlock->elevation_number() > 0)
         {
            description.emplace_back(
               "Elevation",
               fmt::format("{}{}",
                           descriptionBlock->elevation().value(),
                           common::Unicode::kDegree));
         }
      }
   }

   return description;
}

void Level3ProductView::LoadColorTable(
   std::shared_ptr<common::ColorTable> colorTable)
{
   p->colorTable_ = colorTable;
   UpdateColorTableLut();
}

void Level3ProductView::UpdateColorTableLut()
{
   logger_->debug("UpdateColorTable()");

   if (p->graphicMessage_ == nullptr || //
       p->colorTable_ == nullptr ||     //
       !p->colorTable_->IsValid())
   {
      // Nothing to update
      return;
   }

   std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock> descriptionBlock =
      p->graphicMessage_->description_block();

   if (descriptionBlock == nullptr)
   {
      // No description block
      return;
   }

   float         offset    = descriptionBlock->offset();
   float         scale     = descriptionBlock->scale();
   float         logOffset = descriptionBlock->log_offset();
   float         logScale  = descriptionBlock->log_scale();
   std::uint16_t logStart  = descriptionBlock->log_start();
   std::uint8_t  threshold = static_cast<std::uint8_t>(
      std::clamp<std::uint16_t>(descriptionBlock->threshold(),
                                std::numeric_limits<std::uint8_t>::min(),
                                std::numeric_limits<std::uint8_t>::max()));

   // If the threshold is 2, the range min should be set to 1 for range
   // folding
   std::uint8_t  rangeMin       = std::min<std::uint8_t>(1, threshold);
   std::uint16_t numberOfLevels = descriptionBlock->number_of_levels();
   std::uint8_t  rangeMax       = static_cast<std::uint8_t>(
      std::clamp<std::uint16_t>((numberOfLevels > 0) ? numberOfLevels - 1 : 0,
                                std::numeric_limits<std::uint8_t>::min(),
                                std::numeric_limits<std::uint8_t>::max()));

   if (p->savedColorTable_ == p->colorTable_ && //
       p->savedOffset_ == offset &&             //
       p->savedScale_ == scale &&               //
       p->savedLogOffset_ == logOffset &&       //
       p->savedLogScale_ == logScale &&         //
       p->savedLogStart_ == logStart &&         //
       numberOfLevels > 16)
   {
      // The color table LUT does not need updated
      return;
   }

   // Iterate over [rangeMin, numberOfLevels)
   boost::integer_range<uint16_t> dataRange =
      boost::irange<uint16_t>(rangeMin, numberOfLevels);

   std::vector<boost::gil::rgba8_pixel_t>& lut = p->colorTableLut_;
   lut.resize(numberOfLevels - rangeMin);
   lut.shrink_to_fit();

   std::for_each(
      std::execution::par_unseq,
      dataRange.begin(),
      dataRange.end(),
      [&](uint16_t i)
      {
         const size_t lutIndex = i - *dataRange.begin();

         std::optional<float> f = descriptionBlock->data_value(i);

         // Different products use different scale/offset formulas
         if (numberOfLevels > 16 || !descriptionBlock->IsDataLevelCoded())
         {
            if (i == RANGE_FOLDED && threshold > RANGE_FOLDED)
            {
               lut[lutIndex] = p->colorTable_->rf_color();
            }
            else
            {
               if (f.has_value())
               {
                  lut[lutIndex] = p->colorTable_->Color(f.value());
               }
               else
               {
                  lut[lutIndex] = boost::gil::rgba8_pixel_t {0, 0, 0, 0};
               }
            }
         }
         else
         {
            std::optional<wsr88d::DataLevelCode> dataLevelCode =
               descriptionBlock->data_level_code(i);

            if (dataLevelCode == wsr88d::DataLevelCode::RangeFolded)
            {
               lut[lutIndex] = p->colorTable_->rf_color();
            }
            else if (f.has_value())
            {
               lut[lutIndex] = p->colorTable_->Color(f.value());
            }
            else
            {
               lut[lutIndex] = boost::gil::rgba8_pixel_t {0, 0, 0, 0};
            }
         }
      });

   p->colorTableMin_ = rangeMin;
   p->colorTableMax_ = rangeMax;

   p->savedColorTable_ = p->colorTable_;
   p->savedOffset_     = offset;
   p->savedScale_      = scale;
   p->savedLogOffset_  = logOffset;
   p->savedLogScale_   = logScale;
   p->savedLogStart_   = logStart;

   Q_EMIT ColorTableLutUpdated();
}

std::optional<wsr88d::DataLevelCode>
Level3ProductView::GetDataLevelCode(std::uint16_t level) const
{
   if (level > std::numeric_limits<std::uint8_t>::max())
   {
      return std::nullopt;
   }

   auto gpm = graphic_product_message();
   if (gpm == nullptr)
   {
      return std::nullopt;
   }

   std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock> descriptionBlock =
      gpm->description_block();
   if (descriptionBlock == nullptr)
   {
      return std::nullopt;
   }

   return descriptionBlock->data_level_code(static_cast<std::uint8_t>(level));
}

std::optional<float> Level3ProductView::GetDataValue(std::uint16_t level) const
{
   if (level > std::numeric_limits<std::uint8_t>::max())
   {
      return std::nullopt;
   }

   auto gpm = graphic_product_message();
   if (gpm == nullptr)
   {
      return std::nullopt;
   }

   std::shared_ptr<wsr88d::rpg::ProductDescriptionBlock> descriptionBlock =
      gpm->description_block();
   if (descriptionBlock == nullptr)
   {
      return std::nullopt;
   }

   return descriptionBlock->data_value(static_cast<std::uint8_t>(level));
}

bool Level3ProductView::IgnoreUnits() const
{
   // Don't display units on these products. The current method of displaying
   // units is not accurate for these.
   static const std::unordered_set<std::string> kIgnoreUnitsProducts_ {
      "DAA", "DTA", "DU3", "DU6"};

   return (kIgnoreUnitsProducts_.contains(p->product_));
}

} // namespace view
} // namespace qt
} // namespace scwx
