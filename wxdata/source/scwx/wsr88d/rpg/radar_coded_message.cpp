#include <scwx/wsr88d/rpg/radar_coded_message.hpp>

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
   "[scwx::wsr88d::rpg::radar_coded_message] ";

class RadarCodedMessageImpl
{
public:
   explicit RadarCodedMessageImpl() :
       descriptionBlock_ {0},
       pupSiteIdentifier_ {},
       productCategory_ {},
       rdaSiteIdentifier_ {}
   {
   }
   ~RadarCodedMessageImpl() = default;

   bool LoadBlocks(std::istream& is);

   std::shared_ptr<ProductDescriptionBlock> descriptionBlock_;

   std::string pupSiteIdentifier_;
   std::string productCategory_;
   std::string rdaSiteIdentifier_;
};

RadarCodedMessage::RadarCodedMessage() :
    p(std::make_unique<RadarCodedMessageImpl>())
{
}
RadarCodedMessage::~RadarCodedMessage() = default;

RadarCodedMessage::RadarCodedMessage(RadarCodedMessage&&) noexcept = default;
RadarCodedMessage&
RadarCodedMessage::operator=(RadarCodedMessage&&) noexcept = default;

std::shared_ptr<ProductDescriptionBlock>
RadarCodedMessage::description_block() const
{
   return p->descriptionBlock_;
}

bool RadarCodedMessage::Parse(std::istream& is)
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

bool RadarCodedMessageImpl::LoadBlocks(std::istream& is)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Loading Blocks";

   pupSiteIdentifier_.resize(4);
   productCategory_.resize(5);
   rdaSiteIdentifier_.resize(4);

   is.read(pupSiteIdentifier_.data(), 4);
   is.seekg(1, std::ios_base::cur);
   is.read(productCategory_.data(), 5);
   is.seekg(1, std::ios_base::cur);
   is.read(rdaSiteIdentifier_.data(), 4);

   return !is.eof();
}

std::shared_ptr<RadarCodedMessage>
RadarCodedMessage::Create(Level3MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<RadarCodedMessage> message =
      std::make_shared<RadarCodedMessage>();
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
