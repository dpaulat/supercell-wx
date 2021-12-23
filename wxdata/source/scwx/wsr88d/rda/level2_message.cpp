#include <scwx/wsr88d/rda/level2_message.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ = "[scwx::wsr88d::rda::level2_message] ";

class Level2MessageImpl
{
public:
   explicit Level2MessageImpl() : header_() {};
   ~Level2MessageImpl() = default;

   Level2MessageHeader header_;
};

Level2Message::Level2Message() :
    Message(), p(std::make_unique<Level2MessageImpl>())
{
}
Level2Message::~Level2Message() = default;

Level2Message::Level2Message(Level2Message&&) noexcept = default;
Level2Message& Level2Message::operator=(Level2Message&&) noexcept = default;

bool Level2Message::ValidateMessage(std::istream& is, size_t bytesRead) const
{
   bool   messageValid = true;
   size_t dataSize     = header().message_size() * 2 - header().SIZE;

   if (is.eof())
   {
      BOOST_LOG_TRIVIAL(warning) << logPrefix_ << "Reached end of data stream";
      messageValid = false;
   }
   else if (is.fail())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Could not read from input stream";
      messageValid = false;
   }
   else if (bytesRead != dataSize)
   {
      is.seekg(static_cast<std::streamoff>(dataSize) -
                  static_cast<std::streamoff>(bytesRead),
               std::ios_base::cur);

      if (bytesRead < dataSize)
      {
         BOOST_LOG_TRIVIAL(trace)
            << logPrefix_ << "Message contents smaller than size: " << bytesRead
            << " < " << dataSize << " bytes";
      }
      if (bytesRead > dataSize)
      {
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Message contents larger than size: " << bytesRead
            << " > " << dataSize << " bytes";
         messageValid = false;
      }
   }

   return messageValid;
}

const Level2MessageHeader& Level2Message::header() const
{
   return p->header_;
}

void Level2Message::set_header(Level2MessageHeader&& header)
{
   p->header_ = std::move(header);
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
