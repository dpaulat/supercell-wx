#include <scwx/wsr88d/rpg/text_and_special_symbol_packet.hpp>
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
   "scwx::wsr88d::rpg::text_and_special_symbol_packet";
static const auto logger_ = util::Logger::Create(logPrefix_);

class TextAndSpecialSymbolPacket::Impl
{
public:
   explicit Impl() {}
   ~Impl() = default;

   std::uint16_t packetCode_ {0};
   std::uint16_t lengthOfBlock_ {0};
   std::uint16_t valueOfText_ {0};
   std::int16_t  startI_ {0};
   std::int16_t  startJ_ {0};

   std::string text_ {};
};

TextAndSpecialSymbolPacket::TextAndSpecialSymbolPacket() :
    p(std::make_unique<Impl>())
{
}
TextAndSpecialSymbolPacket::~TextAndSpecialSymbolPacket() = default;

TextAndSpecialSymbolPacket::TextAndSpecialSymbolPacket(
   TextAndSpecialSymbolPacket&&) noexcept = default;
TextAndSpecialSymbolPacket& TextAndSpecialSymbolPacket::operator=(
   TextAndSpecialSymbolPacket&&) noexcept = default;

std::uint16_t TextAndSpecialSymbolPacket::packet_code() const
{
   return p->packetCode_;
}

std::uint16_t TextAndSpecialSymbolPacket::length_of_block() const
{
   return p->lengthOfBlock_;
}

std::optional<std::uint16_t> TextAndSpecialSymbolPacket::value_of_text() const
{
   std::optional<std::uint16_t> value;

   if (p->packetCode_ == 8)
   {
      value = p->valueOfText_;
   }

   return value;
}

std::int16_t TextAndSpecialSymbolPacket::start_i() const
{
   return p->startI_;
}

std::int16_t TextAndSpecialSymbolPacket::start_j() const
{
   return p->startJ_;
}

std::string TextAndSpecialSymbolPacket::text() const
{
   return p->text_;
}

SpecialSymbol TextAndSpecialSymbolPacket::special_symbol() const
{
   SpecialSymbol symbol = SpecialSymbol::None;

   if (!p->text_.empty())
   {
      switch (p->text_.at(0))
      {
      case '!': // 0x21
         symbol = SpecialSymbol::PastStormCellPosition;
         break;

      case '"': // 0x22
         symbol = SpecialSymbol::CurrentStormCellPosition;
         break;

      case '#': // 0x23
         symbol = SpecialSymbol::ForecastStormCellPosition;
         break;

      case '$': // 0x24
         symbol = SpecialSymbol::PastMdaPosition;
         break;

      case '%': // 0x25
         symbol = SpecialSymbol::ForecastMdaPosition;
         break;

      default:
         break;
      }
   }

   return symbol;
}

std::size_t TextAndSpecialSymbolPacket::data_size() const
{
   return p->lengthOfBlock_ + 4u;
}

bool TextAndSpecialSymbolPacket::Parse(std::istream& is)
{
   bool blockValid = true;

   std::streampos isBegin = is.tellg();

   is.read(reinterpret_cast<char*>(&p->packetCode_), 2);
   is.read(reinterpret_cast<char*>(&p->lengthOfBlock_), 2);

   p->packetCode_    = ntohs(p->packetCode_);
   p->lengthOfBlock_ = ntohs(p->lengthOfBlock_);

   int textLength = static_cast<int>(p->lengthOfBlock_) - 4;

   is.read(reinterpret_cast<char*>(&p->startI_), 2);
   is.read(reinterpret_cast<char*>(&p->startJ_), 2);

   p->startI_ = ntohs(p->startI_);
   p->startJ_ = ntohs(p->startJ_);

   if (is.eof())
   {
      logger_->debug("Reached end of file");
      blockValid = false;
   }
   else
   {
      if (p->packetCode_ != 1 && p->packetCode_ != 2 && p->packetCode_ != 8)
      {
         logger_->warn("Invalid packet code: {}", p->packetCode_);
         blockValid = false;
      }
      else if (p->lengthOfBlock_ < 1 || p->lengthOfBlock_ > 32767)
      {
         logger_->warn("Invalid length of block: {}", p->lengthOfBlock_);
         blockValid = false;
      }
      else if (p->packetCode_ == 8)
      {
         is.read(reinterpret_cast<char*>(&p->valueOfText_), 2);
         p->valueOfText_ = ntohs(p->valueOfText_);

         textLength -= 2;
      }
   }

   if (blockValid && textLength < 0)
   {
      logger_->warn("Too few bytes in block: {}", p->lengthOfBlock_);
      blockValid = false;
   }

   if (blockValid)
   {
      p->text_.resize(textLength);
      is.read(p->text_.data(), textLength);
   }

   std::streampos isEnd = is.tellg();

   if (!ValidateMessage(is, isEnd - isBegin))
   {
      blockValid = false;
   }

   return blockValid;
}

std::shared_ptr<TextAndSpecialSymbolPacket>
TextAndSpecialSymbolPacket::Create(std::istream& is)
{
   std::shared_ptr<TextAndSpecialSymbolPacket> packet =
      std::make_shared<TextAndSpecialSymbolPacket>();

   if (!packet->Parse(is))
   {
      packet.reset();
   }

   return packet;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
