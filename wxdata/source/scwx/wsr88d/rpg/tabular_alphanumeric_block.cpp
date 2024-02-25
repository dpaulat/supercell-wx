#include <scwx/wsr88d/rpg/tabular_alphanumeric_block.hpp>
#include <scwx/wsr88d/rpg/level3_message_header.hpp>
#include <scwx/wsr88d/rpg/packet_factory.hpp>
#include <scwx/wsr88d/rpg/product_description_block.hpp>
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
   "scwx::wsr88d::rpg::tabular_alphanumeric_block";
static const auto logger_ = util::Logger::Create(logPrefix_);

class TabularAlphanumericBlockImpl
{
public:
   explicit TabularAlphanumericBlockImpl() :
       blockDivider1_ {0},
       blockId_ {0},
       lengthOfBlock_ {0},
       messageHeader_ {},
       descriptionBlock_ {},
       blockDivider2_ {0},
       numberOfPages_ {0},
       pageList_ {}
   {
   }
   ~TabularAlphanumericBlockImpl() = default;

   int16_t  blockDivider1_;
   int16_t  blockId_;
   uint32_t lengthOfBlock_;

   std::shared_ptr<Level3MessageHeader>     messageHeader_;
   std::shared_ptr<ProductDescriptionBlock> descriptionBlock_;

   int16_t  blockDivider2_;
   uint16_t numberOfPages_;

   std::vector<std::vector<std::string>> pageList_;
};

TabularAlphanumericBlock::TabularAlphanumericBlock() :
    Message(), p(std::make_unique<TabularAlphanumericBlockImpl>())
{
}
TabularAlphanumericBlock::~TabularAlphanumericBlock() = default;

TabularAlphanumericBlock::TabularAlphanumericBlock(
   TabularAlphanumericBlock&&) noexcept = default;
TabularAlphanumericBlock& TabularAlphanumericBlock::operator=(
   TabularAlphanumericBlock&&) noexcept = default;

int16_t TabularAlphanumericBlock::block_divider() const
{
   return p->blockDivider1_;
}

size_t TabularAlphanumericBlock::data_size() const
{
   return p->lengthOfBlock_;
}

const std::vector<std::vector<std::string>>&
TabularAlphanumericBlock::page_list() const
{
   return p->pageList_;
}

bool TabularAlphanumericBlock::Parse(std::istream& is)
{
   return Parse(is, false);
}

bool TabularAlphanumericBlock::Parse(std::istream& is, bool skipHeader)
{
   bool blockValid = true;

   const std::streampos blockStart = is.tellg();

   if (!skipHeader)
   {

      is.read(reinterpret_cast<char*>(&p->blockDivider1_), 2);
      is.read(reinterpret_cast<char*>(&p->blockId_), 2);
      is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 4);

      p->blockDivider1_ = ntohs(p->blockDivider1_);
      p->blockId_       = ntohs(p->blockId_);
      p->lengthOfBlock_ = ntohl(p->lengthOfBlock_);

      if (is.eof())
      {
         logger_->debug("Reached end of file");
         blockValid = false;
      }
      else
      {
         if (p->blockDivider1_ != -1)
         {
            logger_->warn("Invalid first block divider: {}", p->blockDivider1_);
            blockValid = false;
         }
         if (p->blockId_ != 3)
         {
            logger_->warn("Invalid block ID: {}", p->blockId_);
            blockValid = false;
         }
         if (p->lengthOfBlock_ < 10)
         {
            logger_->warn("Invalid block length: {}", p->lengthOfBlock_);
            blockValid = false;
         }
      }

      if (blockValid)
      {
         p->messageHeader_ = std::make_shared<Level3MessageHeader>();
         blockValid        = p->messageHeader_->Parse(is);

         if (!blockValid)
         {
            p->messageHeader_ = nullptr;
         }
      }

      if (blockValid)
      {
         p->descriptionBlock_ = std::make_shared<ProductDescriptionBlock>();
         blockValid           = p->descriptionBlock_->Parse(is);

         if (!blockValid)
         {
            p->descriptionBlock_ = nullptr;
         }
      }
   }

   if (blockValid)
   {
      is.read(reinterpret_cast<char*>(&p->blockDivider2_), 2);
      is.read(reinterpret_cast<char*>(&p->numberOfPages_), 2);

      p->blockDivider2_ = ntohs(p->blockDivider2_);
      p->numberOfPages_ = ntohs(p->numberOfPages_);

      if (p->blockDivider2_ != -1)
      {
         logger_->warn("Invalid second block divider: {}", p->blockDivider2_);
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
      uint16_t numberOfCharacters;

      for (uint16_t i = 0; i < p->numberOfPages_; i++)
      {
         std::vector<std::string> lineList;

         while (!is.eof())
         {
            is.read(reinterpret_cast<char*>(&numberOfCharacters), 2);
            numberOfCharacters = ntohs(numberOfCharacters);

            if (static_cast<int16_t>(numberOfCharacters) == -1)
            {
               // End of page
               break;
            }
            else if (numberOfCharacters > 80)
            {
               logger_->warn("Invalid number of characters: {} (Page {})",
                             numberOfCharacters,
                             (i + 1));
               blockValid = false;
               break;
            }

            std::string line;
            line.resize(numberOfCharacters);
            is.read(line.data(), numberOfCharacters);

            lineList.push_back(std::move(line));
         }

         p->pageList_.push_back(std::move(lineList));
      }
   }

   const std::streampos blockEnd = is.tellg();

   if (!skipHeader && !ValidateMessage(is, blockEnd - blockStart))
   {
      blockValid = false;
   }
   else if (skipHeader && is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }

   return blockValid;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
