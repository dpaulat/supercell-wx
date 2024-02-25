#include <scwx/wsr88d/rpg/graphic_alphanumeric_block.hpp>
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
   "scwx::wsr88d::rpg::graphic_alphanumeric_block";
static const auto logger_ = util::Logger::Create(logPrefix_);

class GraphicAlphanumericBlockImpl
{
public:
   explicit GraphicAlphanumericBlockImpl() :
       blockDivider_ {0},
       blockId_ {0},
       lengthOfBlock_ {0},
       numberOfPages_ {0},
       pageList_ {}
   {
   }
   ~GraphicAlphanumericBlockImpl() = default;

   int16_t  blockDivider_;
   int16_t  blockId_;
   uint32_t lengthOfBlock_;
   uint16_t numberOfPages_;

   std::vector<std::vector<std::shared_ptr<Packet>>> pageList_;
};

GraphicAlphanumericBlock::GraphicAlphanumericBlock() :
    Message(), p(std::make_unique<GraphicAlphanumericBlockImpl>())
{
}
GraphicAlphanumericBlock::~GraphicAlphanumericBlock() = default;

GraphicAlphanumericBlock::GraphicAlphanumericBlock(
   GraphicAlphanumericBlock&&) noexcept = default;
GraphicAlphanumericBlock& GraphicAlphanumericBlock::operator=(
   GraphicAlphanumericBlock&&) noexcept = default;

int16_t GraphicAlphanumericBlock::block_divider() const
{
   return p->blockDivider_;
}

size_t GraphicAlphanumericBlock::data_size() const
{
   return p->lengthOfBlock_;
}

const std::vector<std::vector<std::shared_ptr<Packet>>>&
GraphicAlphanumericBlock::page_list() const
{
   return p->pageList_;
}

bool GraphicAlphanumericBlock::Parse(std::istream& is)
{
   bool blockValid = true;

   const std::streampos blockStart = is.tellg();

   is.read(reinterpret_cast<char*>(&p->blockDivider_), 2);
   is.read(reinterpret_cast<char*>(&p->blockId_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 4);
   is.read(reinterpret_cast<char*>(&p->numberOfPages_), 2);

   p->blockDivider_  = ntohs(p->blockDivider_);
   p->blockId_       = ntohs(p->blockId_);
   p->lengthOfBlock_ = ntohl(p->lengthOfBlock_);
   p->numberOfPages_ = ntohs(p->numberOfPages_);

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
      if (p->blockId_ != 2)
      {
         logger_->warn("Invalid block ID: {}", p->blockId_);
         blockValid = false;
      }
      if (p->lengthOfBlock_ < 10)
      {
         logger_->warn("Invalid block length: {}", p->lengthOfBlock_);
         blockValid = false;
      }
      if (p->numberOfPages_ < 1 || p->numberOfPages_ > 48)
      {
         logger_->warn("Invalid number of pages: {}", p->numberOfPages_);
         blockValid = false;
      }
   }

   if (blockValid)
   {
      uint16_t pageNumber;
      uint16_t lengthOfPage;

      for (uint16_t i = 0; i < p->numberOfPages_; i++)
      {
         logger_->trace("Page {}", (i + 1));

         std::vector<std::shared_ptr<Packet>> packetList;
         uint32_t                             bytesRead = 0;

         is.read(reinterpret_cast<char*>(&pageNumber), 2);
         is.read(reinterpret_cast<char*>(&lengthOfPage), 2);

         pageNumber   = ntohs(pageNumber);
         lengthOfPage = ntohs(lengthOfPage);

         std::streampos pageStart = is.tellg();
         std::streampos pageEnd =
            pageStart + static_cast<std::streamoff>(lengthOfPage);

         if (pageNumber != i + 1)
         {
            logger_->warn(
               "Page out of order: Expected {}, found {}", (i + 1), pageNumber);
         }

         while (bytesRead < lengthOfPage)
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

         if (bytesRead < lengthOfPage)
         {
            logger_->trace("Page bytes read smaller than size: {} < {} bytes",
                           bytesRead,
                           lengthOfPage);
            blockValid = false;
            is.seekg(pageEnd, std::ios_base::beg);
         }
         if (bytesRead > lengthOfPage)
         {
            logger_->warn("Page bytes read larger than size: {} > {} bytes",
                          bytesRead,
                          lengthOfPage);
            blockValid = false;
            is.seekg(pageEnd, std::ios_base::beg);
         }

         p->pageList_.push_back(std::move(packetList));
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
