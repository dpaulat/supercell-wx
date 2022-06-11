#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/rangebuf.hpp>

#include <istream>
#include <string>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::graphic_product_message";
static const auto logger_ = util::Logger::Create(logPrefix_);

class GraphicProductMessageImpl
{
public:
   explicit GraphicProductMessageImpl() :
       descriptionBlock_ {0},
       symbologyBlock_ {0},
       graphicBlock_ {0},
       tabularBlock_ {0}
   {
   }
   ~GraphicProductMessageImpl() = default;

   bool LoadBlocks(std::istream& is);

   std::shared_ptr<ProductDescriptionBlock>  descriptionBlock_;
   std::shared_ptr<ProductSymbologyBlock>    symbologyBlock_;
   std::shared_ptr<GraphicAlphanumericBlock> graphicBlock_;
   std::shared_ptr<TabularAlphanumericBlock> tabularBlock_;
};

GraphicProductMessage::GraphicProductMessage() :
    p(std::make_unique<GraphicProductMessageImpl>())
{
}
GraphicProductMessage::~GraphicProductMessage() = default;

GraphicProductMessage::GraphicProductMessage(GraphicProductMessage&&) noexcept =
   default;
GraphicProductMessage&
GraphicProductMessage::operator=(GraphicProductMessage&&) noexcept = default;

std::shared_ptr<ProductDescriptionBlock>
GraphicProductMessage::description_block() const
{
   return p->descriptionBlock_;
}

std::shared_ptr<ProductSymbologyBlock>
GraphicProductMessage::symbology_block() const
{
   return p->symbologyBlock_;
}

std::shared_ptr<GraphicAlphanumericBlock>
GraphicProductMessage::graphic_block() const
{
   return p->graphicBlock_;
}

std::shared_ptr<TabularAlphanumericBlock>
GraphicProductMessage::tabular_block() const
{
   return p->tabularBlock_;
}

bool GraphicProductMessage::Parse(std::istream& is)
{
   bool dataValid = true;

   const std::streampos dataStart = is.tellg();

   p->descriptionBlock_ = std::make_shared<ProductDescriptionBlock>();
   dataValid            = p->descriptionBlock_->Parse(is);

   if (dataValid)
   {
      if (p->descriptionBlock_->IsCompressionEnabled())
      {
         size_t messageLength = header().length_of_message();
         size_t prefixLength =
            Level3MessageHeader::SIZE + ProductDescriptionBlock::SIZE;
         size_t recordSize =
            (messageLength > prefixLength) ? messageLength - prefixLength : 0;

         boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
         util::rangebuf r(is.rdbuf(), recordSize);
         in.push(boost::iostreams::bzip2_decompressor());
         in.push(r);

         try
         {
            std::stringstream ss;
            std::streamsize   bytesCopied = boost::iostreams::copy(in, ss);
            logger_->trace("Decompressed data size = {} bytes", bytesCopied);

            dataValid = p->LoadBlocks(ss);
         }
         catch (const boost::iostreams::bzip2_error& ex)
         {
            logger_->warn("Error decompressing data: {}", ex.what());

            dataValid = false;
         }
      }
      else
      {
         dataValid = p->LoadBlocks(is);
      }
   }

   const std::streampos dataEnd = is.tellg();
   if (!ValidateMessage(is, dataEnd - dataStart))
   {
      dataValid = false;
   }

   return dataValid;
}

bool GraphicProductMessageImpl::LoadBlocks(std::istream& is)
{
   bool symbologyValid = true;
   bool graphicValid   = true;
   bool tabularValid   = true;

   logger_->debug("Loading Blocks");

   std::streampos offsetBasePos = is.tellg();

   constexpr size_t offsetBase =
      Level3MessageHeader::SIZE + ProductDescriptionBlock::SIZE;

   size_t offsetToSymbology = descriptionBlock_->offset_to_symbology() * 2u;
   size_t offsetToGraphic   = descriptionBlock_->offset_to_graphic() * 2u;
   size_t offsetToTabular   = descriptionBlock_->offset_to_tabular() * 2u;

   if (offsetToSymbology >= offsetBase)
   {
      symbologyBlock_ = std::make_shared<ProductSymbologyBlock>();

      is.seekg(offsetToSymbology - offsetBase, std::ios_base::cur);
      symbologyValid = symbologyBlock_->Parse(is);
      is.seekg(offsetBasePos, std::ios_base::beg);

      logger_->debug("Product symbology block valid: {}", symbologyValid);

      if (!symbologyValid)
      {
         symbologyBlock_ = nullptr;
      }
   }

   if (offsetToGraphic >= offsetBase)
   {
      graphicBlock_ = std::make_shared<GraphicAlphanumericBlock>();

      is.seekg(offsetToGraphic - offsetBase, std::ios_base::cur);
      graphicValid = graphicBlock_->Parse(is);
      is.seekg(offsetBasePos, std::ios_base::beg);

      logger_->debug("Graphic alphanumeric block valid: {}", graphicValid);

      if (!graphicValid)
      {
         graphicBlock_ = nullptr;
      }
   }

   if (offsetToTabular >= offsetBase)
   {
      tabularBlock_ = std::make_shared<TabularAlphanumericBlock>();

      is.seekg(offsetToTabular - offsetBase, std::ios_base::cur);
      tabularValid = tabularBlock_->Parse(is);
      is.seekg(offsetBasePos, std::ios_base::beg);

      logger_->debug("Tabular alphanumeric block valid: {}", tabularValid);

      if (!tabularValid)
      {
         tabularBlock_ = nullptr;
      }
   }

   return (symbologyValid && graphicValid && tabularValid);
}

std::shared_ptr<GraphicProductMessage>
GraphicProductMessage::Create(Level3MessageHeader&& header, std::istream& is)
{
   std::shared_ptr<GraphicProductMessage> message =
      std::make_shared<GraphicProductMessage>();
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
