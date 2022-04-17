#include <scwx/wsr88d/rpg/product_symbology_block.hpp>
#include <scwx/wsr88d/rpg/packet_factory.hpp>
#include <scwx/util/logger.hpp>

#include <istream>
#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rpg::product_symbology_block";
static const auto logger_ = util::Logger::Create(logPrefix_);

class ProductSymbologyBlockImpl
{
public:
   explicit ProductSymbologyBlockImpl() :
       blockDivider_ {0},
       blockId_ {0},
       lengthOfBlock_ {0},
       numberOfLayers_ {0},
       layerList_ {}
   {
   }
   ~ProductSymbologyBlockImpl() = default;

   int16_t  blockDivider_;
   int16_t  blockId_;
   uint32_t lengthOfBlock_;
   uint16_t numberOfLayers_;

   std::vector<std::vector<std::shared_ptr<Packet>>> layerList_;
};

ProductSymbologyBlock::ProductSymbologyBlock() :
    Message(), p(std::make_unique<ProductSymbologyBlockImpl>())
{
}
ProductSymbologyBlock::~ProductSymbologyBlock() = default;

ProductSymbologyBlock::ProductSymbologyBlock(ProductSymbologyBlock&&) noexcept =
   default;
ProductSymbologyBlock&
ProductSymbologyBlock::operator=(ProductSymbologyBlock&&) noexcept = default;

int16_t ProductSymbologyBlock::block_divider() const
{
   return p->blockDivider_;
}

uint16_t ProductSymbologyBlock::number_of_layers() const
{
   return p->numberOfLayers_;
}

std::vector<std::shared_ptr<Packet>>
ProductSymbologyBlock::packet_list(uint16_t i) const
{
   return p->layerList_[i];
}

size_t ProductSymbologyBlock::data_size() const
{
   return p->lengthOfBlock_;
}

bool ProductSymbologyBlock::Parse(std::istream& is)
{
   bool blockValid = true;

   const std::streampos blockStart = is.tellg();

   is.read(reinterpret_cast<char*>(&p->blockDivider_), 2);
   is.read(reinterpret_cast<char*>(&p->blockId_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 4);
   is.read(reinterpret_cast<char*>(&p->numberOfLayers_), 2);

   p->blockDivider_   = ntohs(p->blockDivider_);
   p->blockId_        = ntohs(p->blockId_);
   p->lengthOfBlock_  = ntohl(p->lengthOfBlock_);
   p->numberOfLayers_ = ntohs(p->numberOfLayers_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else
   {
      if (p->blockDivider_ != -1)
      {
         logger_->warn("Invalid block divider: {}", p->blockDivider_);
         blockValid = false;
      }
      if (p->blockId_ != 1)
      {
         logger_->warn("Invalid block ID: {}", p->blockId_);
         blockValid = false;
      }
      if (p->lengthOfBlock_ < 10)
      {
         logger_->warn("Invalid block length: {}", p->lengthOfBlock_);
         blockValid = false;
      }
      if (p->numberOfLayers_ < 1 || p->numberOfLayers_ > 18)
      {
         logger_->warn("Invalid number of layers: {}", p->numberOfLayers_);
         blockValid = false;
      }
   }

   if (blockValid)
   {
      int16_t  layerDivider;
      uint32_t lengthOfDataLayer;

      for (uint16_t i = 0; i < p->numberOfLayers_; i++)
      {
         logger_->trace("Layer {}", i);

         std::vector<std::shared_ptr<Packet>> packetList;
         uint32_t                             bytesRead = 0;

         is.read(reinterpret_cast<char*>(&layerDivider), 2);
         is.read(reinterpret_cast<char*>(&lengthOfDataLayer), 4);

         layerDivider      = ntohs(layerDivider);
         lengthOfDataLayer = ntohl(lengthOfDataLayer);

         std::streampos layerStart = is.tellg();
         std::streampos layerEnd =
            layerStart + static_cast<std::streamoff>(lengthOfDataLayer);

         while (bytesRead < lengthOfDataLayer)
         {
            std::shared_ptr<Packet> packet = PacketFactory::Create(is);
            if (packet != nullptr)
            {
               packetList.push_back(packet);
               bytesRead += static_cast<uint32_t>(packet->data_size());
            }
            else
            {
               break;
            }
         }

         if (bytesRead < lengthOfDataLayer)
         {
            logger_->trace("Layer bytes read smaller than size: {} < {} bytes",
                           bytesRead,
                           lengthOfDataLayer);
            blockValid = false;
            is.seekg(layerEnd, std::ios_base::beg);
         }
         if (bytesRead > lengthOfDataLayer)
         {
            logger_->warn("Layer bytes read larger than size: {} > {} bytes",
                          bytesRead,
                          lengthOfDataLayer);
            blockValid = false;
            is.seekg(layerEnd, std::ios_base::beg);
         }

         p->layerList_.push_back(std::move(packetList));
      }
   }

   const std::streampos blockEnd = is.tellg();
   if (!ValidateMessage(is, blockEnd - blockStart))
   {
      blockValid = false;
   }

   return blockValid;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
