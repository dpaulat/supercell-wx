#include <scwx/wsr88d/message.hpp>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "[scwx::wsr88d::message] ";

class MessageImpl
{
public:
   explicit MessageImpl() {};
   ~MessageImpl() = default;
};

Message::Message() : p(std::make_unique<MessageImpl>()) {}
Message::~Message() = default;

Message::Message(Message&&) noexcept = default;
Message& Message::operator=(Message&&) noexcept = default;

bool Message::ValidateMessage(std::istream& is, size_t bytesRead) const
{
   bool messageValid = true;

   const size_t dataSize = data_size();

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

} // namespace wsr88d
} // namespace scwx
