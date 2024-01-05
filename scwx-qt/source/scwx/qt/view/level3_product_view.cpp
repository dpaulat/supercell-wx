#include <scwx/qt/view/level3_product_view.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

#include <limits>

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
   float                               savedScale_;
   float                               savedOffset_;
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
            scwx::util::TimeString(volumeTime, currentZone, false));
         description.emplace_back(
            "Product Time",
            scwx::util::TimeString(productTime, currentZone, false));

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

   std::int16_t productCode = descriptionBlock->product_code();
   float        offset      = descriptionBlock->offset();
   float        scale       = descriptionBlock->scale();
   std::uint8_t threshold   = static_cast<std::uint8_t>(
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
         if (numberOfLevels > 16 || productCode == 34)
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
            uint16_t th = descriptionBlock->data_level_threshold(i);
            if ((th & 0x8000u) == 0)
            {
               // If bit 0 is zero, then the LSB is numeric
               if (f.has_value())
               {
                  lut[lutIndex] = p->colorTable_->Color(f.value());
               }
               else
               {
                  lut[lutIndex] = boost::gil::rgba8_pixel_t {0, 0, 0, 0};
               }
            }
            else
            {
               // If bit 0 is one, then the LSB is coded
               uint16_t lsb = th & 0x00ffu;

               switch (lsb)
               {
               case 3: // RF
                  lut[lutIndex] = p->colorTable_->rf_color();
                  break;

               default: // Ignore other values
                  lut[lutIndex] = boost::gil::rgba8_pixel_t {0, 0, 0, 0};
                  break;
               }
            }
         }
      });

   p->colorTableMin_ = rangeMin;
   p->colorTableMax_ = rangeMax;

   p->savedColorTable_ = p->colorTable_;
   p->savedOffset_     = offset;
   p->savedScale_      = scale;

   Q_EMIT ColorTableLutUpdated();
}

} // namespace view
} // namespace qt
} // namespace scwx
