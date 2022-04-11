#include <scwx/qt/view/level3_product_view.hpp>
#include <scwx/common/constants.hpp>
#include <scwx/util/threads.hpp>
#include <scwx/util/time.hpp>
#include <scwx/wsr88d/rpg/digital_radial_data_array_packet.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/radial_data_packet.hpp>

#include <boost/log/trivial.hpp>
#include <boost/range/irange.hpp>
#include <boost/timer/timer.hpp>

namespace scwx
{
namespace qt
{
namespace view
{

static const std::string logPrefix_ = "[scwx::qt::view::level3_product_view] ";

static constexpr uint16_t RANGE_FOLDED = 1u;

class Level3ProductViewImpl
{
public:
   explicit Level3ProductViewImpl(const std::string& product) :
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
   ~Level3ProductViewImpl() = default;

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

Level3ProductView::Level3ProductView(const std::string& product) :
    p(std::make_unique<Level3ProductViewImpl>(product))
{
}
Level3ProductView::~Level3ProductView() = default;

const std::vector<boost::gil::rgba8_pixel_t>&
Level3ProductView::color_table() const
{
   if (p->colorTableLut_.size() == 0)
   {
      return RadarProductView::color_table();
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

void Level3ProductView::LoadColorTable(
   std::shared_ptr<common::ColorTable> colorTable)
{
   p->colorTable_ = colorTable;
   UpdateColorTable();
}

void Level3ProductView::Update()
{
   util::async([=]() { ComputeSweep(); });
}

void Level3ProductView::UpdateColorTable()
{
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

   int16_t  productCode = descriptionBlock->product_code();
   float    offset      = descriptionBlock->offset();
   float    scale       = descriptionBlock->scale();
   uint16_t threshold   = descriptionBlock->threshold();

   // If the threshold is 2, the range min should be set to 1 for range folding
   uint16_t rangeMin       = std::min<uint16_t>(1, threshold);
   uint16_t numberOfLevels = descriptionBlock->number_of_levels();
   uint16_t rangeMax       = (numberOfLevels > 0) ? numberOfLevels - 1 : 0;

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
         float        f;

         // Different products use different scale/offset formulas
         if (numberOfLevels > 16 || productCode == 34)
         {
            if (i == RANGE_FOLDED && threshold > RANGE_FOLDED)
            {
               lut[lutIndex] = p->colorTable_->rf_color();
            }
            else
            {
               switch (descriptionBlock->product_code())
               {
               case 159:
               case 161:
               case 163:
               case 167:
               case 168:
               case 170:
               case 172:
               case 173:
               case 174:
               case 175:
               case 176:
                  f = (i - offset) / scale;
                  break;

               default:
                  f = i * scale + offset;
                  break;
               }

               lut[lutIndex] = p->colorTable_->Color(f);
            }
         }
         else
         {
            uint16_t th = descriptionBlock->data_level_threshold(i);
            if ((th & 0x8000u) == 0)
            {
               float scale = 1.0f;

               if (th & 0x4000u)
               {
                  scale *= 0.01f;
               }
               if (th & 0x2000u)
               {
                  scale *= 0.05f;
               }
               if (th & 0x1000u)
               {
                  scale *= 0.1f;
               }
               if (th & 0x0100u)
               {
                  scale *= -1.0f;
               }

               // If bit 0 is zero, then the LSB is numeric
               f = static_cast<float>(th & 0x00ffu) * scale;

               lut[lutIndex] = p->colorTable_->Color(f);
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

   emit ColorTableUpdated();
}

} // namespace view
} // namespace qt
} // namespace scwx
