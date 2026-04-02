#include <scwx/awips/message.hpp>
#include <scwx/util/logger.hpp>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::message";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class Message::Impl
{
public:
   explicit Impl() = default;
   ~Impl()         = default;

   Impl(const Impl&)             = delete;
   Impl& operator=(const Impl&)  = delete;
   Impl(const Impl&&)            = delete;
   Impl& operator=(const Impl&&) = delete;
};

Message::Message() : p(std::make_unique<Impl>()) {}
Message::~Message() = default;

Message::Message(Message&&) noexcept            = default;
Message& Message::operator=(Message&&) noexcept = default;

bool Message::ValidateMessage(std::istream& is, size_t bytesRead) const
{
   bool messageValid = true;

   const size_t dataSize = data_size();

   if (is.eof())
   {
      logger_->warn("Reached end of data stream");
      messageValid = false;
   }
   else if (is.fail())
   {
      logger_->warn("Could not read from input stream");
      messageValid = false;
   }
   else if (bytesRead != dataSize)
   {
      is.seekg(static_cast<std::streamoff>(dataSize) -
                  static_cast<std::streamoff>(bytesRead),
               std::ios_base::cur);

      if (bytesRead < dataSize)
      {
         logger_->trace("Message contents smaller than size: {} < {} bytes",
                        bytesRead,
                        dataSize);
      }
      if (bytesRead > dataSize)
      {
         logger_->warn("Message contents larger than size: {} > {} bytes",
                       bytesRead,
                       dataSize);
      }
   }

   return messageValid;
}

} // namespace awips
} // namespace scwx
