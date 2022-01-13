#include <scwx/wsr88d/rpg/tabular_product_message.hpp>

#include <istream>
#include <string>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::tabular_product_message] ";

class TabularProductMessageImpl
{
public:
   explicit TabularProductMessageImpl() :
       descriptionBlock_ {0}, tabularBlock_ {0}
   {
   }
   ~TabularProductMessageImpl() = default;

   bool LoadBlocks(std::istream& is);

   std::shared_ptr<ProductDescriptionBlock>  descriptionBlock_;
   std::shared_ptr<TabularAlphanumericBlock> tabularBlock_;
};

TabularProductMessage::TabularProductMessage() :
    p(std::make_unique<TabularProductMessageImpl>())
{
}
TabularProductMessage::~TabularProductMessage() = default;

TabularProductMessage::TabularProductMessage(TabularProductMessage&&) noexcept =
   default;
TabularProductMessage&
TabularProductMessage::operator=(TabularProductMessage&&) noexcept = default;

std::shared_ptr<ProductDescriptionBlock>
TabularProductMessage::description_block() const
{
   return p->descriptionBlock_;
}

std::shared_ptr<TabularAlphanumericBlock>
TabularProductMessage::tabular_block() const
{
   return p->tabularBlock_;
}

bool TabularProductMessage::Parse(std::istream& is)
{
   bool dataValid = true;

   const std::streampos dataStart = is.tellg();

   p->descriptionBlock_ = std::make_shared<ProductDescriptionBlock>();
   dataValid            = p->descriptionBlock_->Parse(is);

   if (dataValid)
   {
      dataValid = p->LoadBlocks(is);
   }

   const std::streampos dataEnd = is.tellg();
   if (!ValidateMessage(is, dataEnd - dataStart))
   {
      dataValid = false;
   }

   return dataValid;
}

bool TabularProductMessageImpl::LoadBlocks(std::istream& is)
{
   constexpr bool skipTabularHeader = true;

   bool tabularValid = true;

   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Loading Blocks";

   std::streampos offsetBasePos = is.tellg();

   constexpr size_t offsetBase =
      Level3MessageHeader::SIZE + ProductDescriptionBlock::SIZE;

   size_t offsetToTabular = descriptionBlock_->offset_to_symbology() * 2u;

   if (offsetToTabular >= offsetBase)
   {
      tabularBlock_ = std::make_shared<TabularAlphanumericBlock>();

      is.seekg(offsetToTabular - offsetBase, std::ios_base::cur);
      tabularValid = tabularBlock_->Parse(is, skipTabularHeader);
      is.seekg(offsetBasePos, std::ios_base::beg);

      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Tabular alphanumeric block valid: " << tabularValid;

      if (!tabularValid)
      {
         tabularBlock_ = nullptr;
      }
   }

   return tabularValid;
}

std::shared_ptr<TabularProductMessage>
TabularProductMessage::Create(Level3MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<TabularProductMessage> message =
      std::make_shared<TabularProductMessage>();
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
