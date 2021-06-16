#include <scwx/wsr88d/rda/message.hpp>

#include <istream>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ = "[scwx::wsr88d::rda::message] ";

class MessageImpl
{
public:
   explicit MessageImpl() : header_() {};
   ~MessageImpl() = default;

   MessageHeader header_;
};

Message::Message() : p(std::make_unique<MessageImpl>()) {}
Message::~Message() = default;

Message::Message(Message&&) noexcept = default;
Message& Message::operator=(Message&&) noexcept = default;

bool Message::ValidateSize(std::istream& is, size_t bytesRead) const
{
   bool   messageValid = true;
   size_t dataSize     = header().message_size() * 2 - header().SIZE;

   if (bytesRead != dataSize)
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

const MessageHeader& Message::header() const
{
   return p->header_;
}

void Message::set_header(MessageHeader&& header)
{
   p->header_ = std::move(header);
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
