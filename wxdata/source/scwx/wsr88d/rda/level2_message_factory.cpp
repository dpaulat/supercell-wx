#include <scwx/wsr88d/rda/level2_message_factory.hpp>

#include <scwx/util/logger.hpp>
#include <scwx/util/vectorbuf.hpp>
#include <scwx/wsr88d/rda/clutter_filter_bypass_map.hpp>
#include <scwx/wsr88d/rda/clutter_filter_map.hpp>
#include <scwx/wsr88d/rda/digital_radar_data.hpp>
#include <scwx/wsr88d/rda/performance_maintenance_data.hpp>
#include <scwx/wsr88d/rda/rda_adaptation_data.hpp>
#include <scwx/wsr88d/rda/rda_status_data.hpp>
#include <scwx/wsr88d/rda/volume_coverage_pattern_data.hpp>

#include <unordered_map>
#include <vector>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

static const std::string logPrefix_ =
   "scwx::wsr88d::rda::level2_message_factory";
static const auto logger_ = util::Logger::Create(logPrefix_);

typedef std::function<std::shared_ptr<Level2Message>(Level2MessageHeader&&,
                                                     std::istream&)>
   CreateLevel2MessageFunction;

static const std::unordered_map<uint8_t, CreateLevel2MessageFunction> create_ {
   {2, RdaStatusData::Create},
   {3, PerformanceMaintenanceData::Create},
   {5, VolumeCoveragePatternData::Create},
   {13, ClutterFilterBypassMap::Create},
   {15, ClutterFilterMap::Create},
   {18, RdaAdaptationData::Create},
   {31, DigitalRadarData::Create}};

static std::vector<char> messageData_;
static size_t            bufferedSize_;
static util::vectorbuf   messageBuffer_(messageData_);
static std::istream      messageBufferStream_(&messageBuffer_);

Level2MessageInfo Level2MessageFactory::Create(std::istream& is)
{
   Level2MessageInfo   info;
   Level2MessageHeader header;
   info.headerValid  = header.Parse(is);
   info.messageValid = info.headerValid;

   if (info.headerValid && create_.find(header.message_type()) == create_.end())
   {
      logger_->warn("Unknown message type: {}",
                    static_cast<unsigned>(header.message_type()));
      info.messageValid = false;
   }

   if (info.messageValid)
   {
      uint16_t segment       = header.message_segment_number();
      uint16_t totalSegments = header.number_of_message_segments();
      uint8_t  messageType   = header.message_type();
      size_t   dataSize = header.message_size() * 2 - Level2MessageHeader::SIZE;

      std::istream* messageStream = nullptr;

      if (totalSegments == 1)
      {
         logger_->trace("Found Message {}", static_cast<unsigned>(messageType));
         messageStream = &is;
      }
      else
      {
         logger_->trace("Found Message {} Segment {}/{}",
                        static_cast<unsigned>(messageType),
                        segment,
                        totalSegments);

         if (segment == 1)
         {
            // Estimate total message size
            messageData_.resize(dataSize * totalSegments);
            messageBufferStream_.clear();
            bufferedSize_ = 0;
         }

         if (messageData_.capacity() < bufferedSize_ + dataSize)
         {
            logger_->debug("Bad size estimate, increasing size");

            // Estimate remaining size
            uint16_t remainingSegments =
               std::max<uint16_t>(totalSegments - segment + 1, 100u);
            size_t remainingSize = remainingSegments * dataSize;

            messageData_.resize(bufferedSize_ + remainingSize);
         }

         is.read(messageData_.data() + bufferedSize_, dataSize);
         bufferedSize_ += dataSize;

         if (is.eof())
         {
            logger_->warn("End of file reached trying to buffer message");
            info.messageValid = false;
            messageData_.shrink_to_fit();
            bufferedSize_ = 0;
         }
         else if (segment == totalSegments)
         {
            messageBuffer_.update_read_pointers(bufferedSize_);
            header.set_message_size(static_cast<uint16_t>(
               bufferedSize_ / 2 + Level2MessageHeader::SIZE));

            messageStream = &messageBufferStream_;
         }
      }

      if (messageStream != nullptr)
      {
         info.message =
            create_.at(messageType)(std::move(header), *messageStream);
         messageData_.shrink_to_fit();
         messageBufferStream_.clear();
         bufferedSize_ = 0;
      }
   }
   else if (info.headerValid)
   {
      // Seek to the end of the current message
      is.seekg(header.message_size() * 2 - rda::Level2MessageHeader::SIZE,
               std::ios_base::cur);
   }

   if (info.message == nullptr)
   {
      info.messageValid = false;
   }

   return info;
}

} // namespace rda
} // namespace wsr88d
} // namespace scwx
