#include <scwx/wsr88d/rpg/level3_message_factory.hpp>

#include <scwx/util/vectorbuf.hpp>
#include <scwx/wsr88d/rpg/general_status_message.hpp>
#include <scwx/wsr88d/rpg/graphic_product_message.hpp>
#include <scwx/wsr88d/rpg/radar_coded_message.hpp>
#include <scwx/wsr88d/rpg/tabular_product_message.hpp>

#include <unordered_map>
#include <vector>

#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ =
   "[scwx::wsr88d::rpg::level3_message_factory] ";

typedef std::function<std::shared_ptr<Level3Message>(Level3MessageHeader&&,
                                                     std::istream&)>
   CreateLevel3MessageFunction;

static const std::unordered_map<int16_t, CreateLevel3MessageFunction> //
   create_ {{2, GeneralStatusMessage::Create},
            {19, GraphicProductMessage::Create},
            {20, GraphicProductMessage::Create},
            {27, GraphicProductMessage::Create},
            {30, GraphicProductMessage::Create},
            {31, GraphicProductMessage::Create},
            {32, GraphicProductMessage::Create},
            {37, GraphicProductMessage::Create},
            {38, GraphicProductMessage::Create},
            {41, GraphicProductMessage::Create},
            {48, GraphicProductMessage::Create},
            {49, GraphicProductMessage::Create},
            {50, GraphicProductMessage::Create},
            {51, GraphicProductMessage::Create},
            {56, GraphicProductMessage::Create},
            {57, GraphicProductMessage::Create},
            {58, GraphicProductMessage::Create},
            {59, GraphicProductMessage::Create},
            {61, GraphicProductMessage::Create},
            {62, TabularProductMessage::Create},
            {65, GraphicProductMessage::Create},
            {66, GraphicProductMessage::Create},
            {67, GraphicProductMessage::Create},
            {74, RadarCodedMessage::Create},
            {75, TabularProductMessage::Create},
            {77, TabularProductMessage::Create},
            {78, GraphicProductMessage::Create},
            {79, GraphicProductMessage::Create},
            {80, GraphicProductMessage::Create},
            {81, GraphicProductMessage::Create},
            {82, TabularProductMessage::Create},
            {84, GraphicProductMessage::Create},
            {86, GraphicProductMessage::Create},
            {90, GraphicProductMessage::Create},
            {93, GraphicProductMessage::Create},
            {94, GraphicProductMessage::Create},
            {97, GraphicProductMessage::Create},
            {98, GraphicProductMessage::Create},
            {99, GraphicProductMessage::Create},
            {100, GraphicProductMessage::Create},
            {101, GraphicProductMessage::Create},
            {102, GraphicProductMessage::Create},
            {104, GraphicProductMessage::Create},
            {105, GraphicProductMessage::Create},
            {107, GraphicProductMessage::Create},
            {108, GraphicProductMessage::Create},
            {109, GraphicProductMessage::Create},
            {110, GraphicProductMessage::Create},
            {111, GraphicProductMessage::Create},
            {113, GraphicProductMessage::Create},
            {132, GraphicProductMessage::Create},
            {133, GraphicProductMessage::Create},
            {134, GraphicProductMessage::Create},
            {135, GraphicProductMessage::Create},
            {137, GraphicProductMessage::Create},
            {138, GraphicProductMessage::Create},
            {140, GraphicProductMessage::Create},
            {141, GraphicProductMessage::Create},
            {143, GraphicProductMessage::Create},
            {144, GraphicProductMessage::Create},
            {145, GraphicProductMessage::Create},
            {146, GraphicProductMessage::Create},
            {147, GraphicProductMessage::Create},
            {149, GraphicProductMessage::Create},
            {150, GraphicProductMessage::Create},
            {151, GraphicProductMessage::Create},
            {152, GraphicProductMessage::Create},
            {153, GraphicProductMessage::Create},
            {154, GraphicProductMessage::Create},
            {155, GraphicProductMessage::Create},
            {159, GraphicProductMessage::Create},
            {161, GraphicProductMessage::Create},
            {163, GraphicProductMessage::Create},
            {165, GraphicProductMessage::Create},
            {166, GraphicProductMessage::Create},
            {167, GraphicProductMessage::Create},
            {168, GraphicProductMessage::Create},
            {169, GraphicProductMessage::Create},
            {170, GraphicProductMessage::Create},
            {171, GraphicProductMessage::Create},
            {172, GraphicProductMessage::Create},
            {173, GraphicProductMessage::Create},
            {174, GraphicProductMessage::Create},
            {175, GraphicProductMessage::Create},
            {176, GraphicProductMessage::Create},
            {177, GraphicProductMessage::Create},
            {178, GraphicProductMessage::Create},
            {179, GraphicProductMessage::Create},
            {193, GraphicProductMessage::Create},
            {195, GraphicProductMessage::Create},
            {196, GraphicProductMessage::Create},
            {202, GraphicProductMessage::Create}};

std::shared_ptr<Level3Message> Level3MessageFactory::Create(std::istream& is)
{
   Level3MessageHeader            header;
   std::shared_ptr<Level3Message> message;

   bool headerValid  = header.Parse(is);
   bool messageValid = headerValid;

   if (headerValid && create_.find(header.message_code()) == create_.end())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Unknown message type: " << header.message_code();
      messageValid = false;
   }

   if (messageValid)
   {
      int16_t messageCode = header.message_code();
      size_t  dataSize = header.length_of_message() - Level3MessageHeader::SIZE;

      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Found Message " << messageCode;

      message = create_.at(messageCode)(std::move(header), is);
   }
   else if (headerValid)
   {
      // Seek to the end of the current message
      is.seekg(header.length_of_message() - Level3MessageHeader::SIZE,
               std::ios_base::cur);
   }

   return message;
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
