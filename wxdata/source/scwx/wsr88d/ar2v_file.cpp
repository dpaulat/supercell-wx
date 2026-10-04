#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/rda/digital_radar_data.hpp>
#include <scwx/wsr88d/rda/level2_message_factory.hpp>
#include <scwx/wsr88d/rda/rda_types.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/rangebuf.hpp>
#include <scwx/util/time.hpp>
#include <scwx/common/geographic.hpp>

#include <fstream>
#include <sstream>
#include <utility>

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable : 4702)
#endif

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif

#include <boost/algorithm/string/trim.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <fmt/chrono.h>

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#endif

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "scwx::wsr88d::ar2v_file";
static const auto        logger_    = util::Logger::Create(logPrefix_);

class Ar2vFileImpl
{
public:
   explicit Ar2vFileImpl() {};
   ~Ar2vFileImpl() = default;

   std::size_t DecompressLDMRecords(std::istream& is);
   void        HandleMessage(std::shared_ptr<rda::Level2Message>& message);
   void        IndexFile();
   void        ParseLDMRecords();
   void        ParseLDMRecord(std::istream& is);
   void ProcessRadarData(const std::shared_ptr<rda::GenericRadarData>& message);

   std::string   tapeFilename_ {};
   std::string   extensionNumber_ {};
   std::uint32_t julianDate_ {0};
   std::uint32_t milliseconds_ {0};
   std::string   icao_ {};

   std::size_t messageCount_ {0};

   std::shared_ptr<rda::VolumeCoveragePatternData>              vcpData_ {};
   std::map<std::uint16_t, std::shared_ptr<rda::ElevationScan>> radarData_ {};

   std::map<rda::DataBlockType,
            std::map<float,
                     std::map<std::chrono::system_clock::time_point,
                              std::shared_ptr<rda::ElevationScan>>>>
      index_ {};

   std::stringstream ldmData_ {};
};

Ar2vFile::Ar2vFile() : p(std::make_unique<Ar2vFileImpl>()) {}
Ar2vFile::~Ar2vFile() = default;

Ar2vFile::Ar2vFile(Ar2vFile&&) noexcept            = default;
Ar2vFile& Ar2vFile::operator=(Ar2vFile&&) noexcept = default;

std::uint32_t Ar2vFile::julian_date() const
{
   return p->julianDate_;
}

std::uint32_t Ar2vFile::milliseconds() const
{
   return p->milliseconds_;
}

std::string Ar2vFile::icao() const
{
   return p->icao_;
}

std::size_t Ar2vFile::message_count() const
{
   return p->messageCount_;
}

std::chrono::system_clock::time_point Ar2vFile::start_time() const
{
   return util::TimePoint(p->julianDate_, p->milliseconds_);
}

std::chrono::system_clock::time_point Ar2vFile::end_time() const
{
   std::chrono::system_clock::time_point endTime {};

   if (p->radarData_.size() > 0)
   {
      std::shared_ptr<rda::GenericRadarData> lastRadial =
         p->radarData_.crbegin()->second->crbegin()->second;

      endTime = util::TimePoint(lastRadial->modified_julian_date(),
                                lastRadial->collection_time());
   }

   return endTime;
}

std::map<std::uint16_t, std::shared_ptr<rda::ElevationScan>>
Ar2vFile::radar_data() const
{
   return p->radarData_;
}

std::shared_ptr<const rda::VolumeCoveragePatternData> Ar2vFile::vcp_data() const
{
   return p->vcpData_;
}

std::tuple<std::shared_ptr<rda::ElevationScan>, float, std::vector<float>>
Ar2vFile::GetElevationScan(rda::DataBlockType                    dataBlockType,
                           float                                 elevation,
                           std::chrono::system_clock::time_point time) const
{
   logger_->trace("GetElevationScan: {} degrees", elevation);

   std::shared_ptr<rda::ElevationScan> elevationScan = nullptr;
   float                               elevationCut  = 0.0f;
   std::vector<float>                  elevationCuts;

   if (p->index_.contains(dataBlockType))
   {
      auto& scans = p->index_.at(dataBlockType);

      float lowerBound = scans.cbegin()->first;
      float upperBound = scans.crbegin()->first;

      // Find closest elevation match
      for (auto& scan : scans)
      {
         if (scan.first > lowerBound && scan.first <= elevation)
         {
            lowerBound = scan.first;
         }
         if (scan.first < upperBound && scan.first >= elevation)
         {
            upperBound = scan.first;
         }

         elevationCuts.push_back(scan.first);
      }

      const float lowerDelta = std::abs(elevation - lowerBound);
      const float upperDelta = std::abs(elevation - upperBound);

      // Select closest elevation match
      elevationCut = (lowerDelta < upperDelta) ? lowerBound : upperBound;

      // Select closest time match, not newer than the selected time
      std::chrono::system_clock::time_point foundTime {};
      auto& elevationScans = scans.at(elevationCut);

      for (auto& scan : elevationScans)
      {
         auto scanTime = std::chrono::floor<std::chrono::seconds>(scan.first);

         if (elevationScan == nullptr ||
             ((scanTime <= time ||
               time == std::chrono::system_clock::time_point {}) &&
              scanTime > foundTime))
         {
            elevationScan = scan.second;
            foundTime     = scanTime;
         }
      }
   }

   return std::tie(elevationScan, elevationCut, elevationCuts);
}

bool Ar2vFile::LoadFile(const std::string& filename)
{
   logger_->debug("LoadFile: {}", filename);
   bool fileValid = true;

   std::ifstream f(filename, std::ios_base::in | std::ios_base::binary);
   if (!f.good())
   {
      logger_->warn("Could not open file for reading: {}", filename);
      fileValid = false;
   }

   if (fileValid)
   {
      fileValid = LoadData(f);
   }

   return fileValid;
}

bool Ar2vFile::LoadData(std::istream& is)
{
   logger_->debug("Loading Data");

   bool dataValid = true;

   // Read Volume Header Record
   p->tapeFilename_.resize(9, ' ');
   p->extensionNumber_.resize(3, ' ');
   p->icao_.resize(4, ' ');

   is.read(&p->tapeFilename_[0], 9);
   is.read(&p->extensionNumber_[0], 3);
   is.read(reinterpret_cast<char*>(&p->julianDate_), 4);
   is.read(reinterpret_cast<char*>(&p->milliseconds_), 4);
   is.read(&p->icao_[0], 4);

   p->julianDate_   = ntohl(p->julianDate_);
   p->milliseconds_ = ntohl(p->milliseconds_);

   if (is.eof())
   {
      logger_->warn("Could not read Volume Header Record");
      dataValid = false;
   }

   // Trim spaces and null characters from the end of the ICAO
   boost::trim_right_if(p->icao_,
                        [](char x) { return std::isspace(x) || x == '\0'; });

   if (dataValid)
   {
      auto timePoint = util::TimePoint(p->julianDate_, p->milliseconds_);

      logger_->debug("Filename:  {}", p->tapeFilename_);
      logger_->debug("Extension: {}", p->extensionNumber_);
      logger_->debug("Date:      {} ({:%Y-%m-%d})", p->julianDate_, timePoint);
      logger_->debug(
         "Time:      {} ({:%H:%M:%S})", p->milliseconds_, timePoint);
      logger_->debug("ICAO:      {}", p->icao_);

      size_t decompressedRecords = p->DecompressLDMRecords(is);
      if (decompressedRecords == 0)
      {
         p->ParseLDMRecord(is);
      }
      else
      {
         p->ParseLDMRecords();
      }
   }

   p->IndexFile();

   return dataValid;
}

std::size_t Ar2vFileImpl::DecompressLDMRecords(std::istream& is)
{
   logger_->trace("Decompressing LDM Records");

   std::size_t numRecords = 0;

   while (is.peek() != EOF)
   {
      std::streampos startPosition = is.tellg();
      std::int32_t   controlWord   = 0;
      std::size_t    recordSize;

      is.read(reinterpret_cast<char*>(&controlWord), 4);

      controlWord = ntohl(controlWord);
      recordSize  = std::abs(controlWord);

      logger_->trace("LDM Record Found: Size = {} bytes", recordSize);

      if (recordSize == 0)
      {
         is.seekg(startPosition, std::ios_base::beg);
         break;
      }

      boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
      util::rangebuf r(is.rdbuf(), recordSize);
      in.push(boost::iostreams::bzip2_decompressor());
      in.push(r);

      try
      {
         std::stringstream decompressedRecord;
         std::streamsize   bytesCopied =
            boost::iostreams::copy(in, decompressedRecord);
         logger_->trace("Decompressed record size = {} bytes", bytesCopied);

         ldmData_ << std::move(decompressedRecord).str();
      }
      catch (const boost::iostreams::bzip2_error& ex)
      {
         logger_->warn(
            "Error decompressing record {}: {}", numRecords, ex.what());

         is.seekg(startPosition + std::streampos(recordSize),
                  std::ios_base::beg);
      }

      ++numRecords;
   }

   logger_->trace("Decompressed {} LDM Records", numRecords);

   return numRecords;
}

void Ar2vFileImpl::ParseLDMRecords()
{
   logger_->trace("Parsing LDM Records");
   ParseLDMRecord(ldmData_);
   ldmData_.clear();
}

void Ar2vFileImpl::ParseLDMRecord(std::istream& is)
{
   static constexpr std::size_t kDefaultSegmentSize = 2432;
   static constexpr std::size_t kCtmHeaderSize      = 12;

   auto ctx = rda::Level2MessageFactory::CreateContext();

   while (!is.eof() && !is.fail())
   {
      // The communications manager inserts an extra 12 bytes at the beginning
      // of each record
      is.seekg(kCtmHeaderSize, std::ios_base::cur);

      // Each message requires 2432 bytes of storage, with the exception of
      // Message Types 29 and 31.
      std::size_t messageSize = kDefaultSegmentSize - kCtmHeaderSize;

      // Mark current position
      std::streampos messageStart = is.tellg();

      // Parse the header
      rda::Level2MessageHeader messageHeader;
      bool                     headerValid = messageHeader.Parse(is);
      is.seekg(messageStart, std::ios_base::beg);

      if (headerValid)
      {
         std::uint8_t messageType = messageHeader.message_type();

         // Each message requires 2432 bytes of storage, with the exception of
         // Message Types 29 and 31.
         if (messageType == 29 || messageType == 31)
         {
            if (messageHeader.message_size() == 65535)
            {
               messageSize = (static_cast<std::size_t>(
                                 messageHeader.number_of_message_segments())
                              << 16) +
                             messageHeader.message_segment_number();
            }
            else
            {
               messageSize =
                  static_cast<std::size_t>(messageHeader.message_size()) * 2;
            }
         }

         // Parse the current message
         rda::Level2MessageInfo msgInfo =
            rda::Level2MessageFactory::Create(is, ctx);

         if (msgInfo.messageValid)
         {
            HandleMessage(msgInfo.message);
         }
      }

      // Skip to next message
      is.seekg(messageStart + static_cast<std::streampos>(messageSize),
               std::ios_base::beg);
   }
}

void Ar2vFileImpl::HandleMessage(std::shared_ptr<rda::Level2Message>& message)
{
   ++messageCount_;

   switch (message->header().message_type())
   {
   case static_cast<std::uint8_t>(rda::MessageId::VolumeCoveragePatternData):
      vcpData_ =
         std::static_pointer_cast<rda::VolumeCoveragePatternData>(message);
      break;

   case static_cast<std::uint8_t>(rda::MessageId::DigitalRadarData):
   case static_cast<std::uint8_t>(rda::MessageId::DigitalRadarDataGeneric):
      ProcessRadarData(
         std::static_pointer_cast<rda::GenericRadarData>(message));
      break;

   default:
      break;
   }
}

void Ar2vFileImpl::ProcessRadarData(
   const std::shared_ptr<rda::GenericRadarData>& message)
{
   std::uint16_t azimuthIndex   = message->azimuth_number() - 1;
   std::uint16_t elevationIndex = message->elevation_number() - 1;

   if (radarData_[elevationIndex] == nullptr)
   {
      radarData_[elevationIndex] = std::make_shared<rda::ElevationScan>();
   }

   (*radarData_[elevationIndex])[azimuthIndex] = message;
}

void Ar2vFileImpl::IndexFile()
{
   logger_->trace("Indexing file");

   for (auto& elevationCut : radarData_)
   {
      float             elevationAngle {};
      rda::WaveformType waveformType = rda::WaveformType::Unknown;

      std::shared_ptr<rda::GenericRadarData>& radial0 =
         (*elevationCut.second)[0];

      if (radial0 == nullptr)
      {
         logger_->warn("Empty radial data");
         continue;
      }

      std::shared_ptr<rda::DigitalRadarData> digitalRadarData0 = nullptr;

      if (vcpData_ != nullptr)
      {
         elevationAngle =
            static_cast<float>(vcpData_->elevation_angle(elevationCut.first));
         waveformType = vcpData_->waveform_type(elevationCut.first);
      }
      else if ((digitalRadarData0 =
                   std::dynamic_pointer_cast<rda::DigitalRadarData>(radial0)) !=
               nullptr)
      {
         elevationAngle = digitalRadarData0->elevation_angle().value();
      }
      else
      {
         // Return here, because we should only have a single message type
         logger_->warn("Cannot index file without VCP data");
         return;
      }

      for (rda::DataBlockType dataBlockType :
           rda::MomentDataBlockTypeIterator())
      {
         if (dataBlockType == rda::DataBlockType::MomentRef &&
             waveformType ==
                rda::WaveformType::ContiguousDopplerWithAmbiguityResolution)
         {
            // Reflectivity data is contained within both surveillance and
            // doppler modes.  Surveillance mode produces a better image.
            continue;
         }

         auto momentData = radial0->moment_data_block(dataBlockType);

         if (momentData != nullptr)
         {
            auto time = util::TimePoint(radial0->modified_julian_date(),
                                        radial0->collection_time());

            index_[dataBlockType][elevationAngle][time] = elevationCut.second;
         }
      }
   }
}

bool Ar2vFile::LoadLDMRecords(std::istream& is)
{
   const size_t decompressedRecords = p->DecompressLDMRecords(is);
   if (decompressedRecords == 0)
   {
      p->ParseLDMRecord(is);
   }
   else
   {
      p->ParseLDMRecords();
   }

   return true;
}

bool Ar2vFile::IndexFile()
{
   p->IndexFile();
   return true;
}

// NOLINTNEXTLINE
bool IsRadarDataIncomplete(
   const std::shared_ptr<const rda::ElevationScan>& radarData)
{
   // Assume the data is incomplete when the delta between the first and last
   // angles is greater than 2.5 degrees.
   constexpr units::degrees<float> kIncompleteDataAngleThreshold_ {2.5};

   const units::degrees<float> firstAngle =
      radarData->cbegin()->second->azimuth_angle();
   const units::degrees<float> lastAngle =
      radarData->crbegin()->second->azimuth_angle();
   const units::degrees<float> angleDelta =
      common::GetAngleDelta(firstAngle, lastAngle);

   return angleDelta > kIncompleteDataAngleThreshold_;
}

Ar2vFile::Ar2vFile(const std::shared_ptr<Ar2vFile>& current,
                   const std::shared_ptr<Ar2vFile>& last) :
    Ar2vFile()
{
   // This is only used to index right now, so not a huge deal
   p->vcpData_ = nullptr;

   // Reconstruct index from the other's indexes
   if (current != nullptr)
   {
      for (const auto& type : current->p->index_)
      {
         for (const auto& elevation : type.second)
         {
            // Get the most recent scan
            const auto& mostRecent = elevation.second.crbegin();
            if (mostRecent == elevation.second.crend())
            {
               continue;
            }

            // Add previous scans for stepping back in time
            for (auto scan = ++(elevation.second.rbegin());
                 scan != elevation.second.rend();
                 ++scan)
            {
               p->index_[type.first][elevation.first][scan->first] =
                  scan->second;
            }

            // Merge this scan with the last one if it is incomplete
            if (IsRadarDataIncomplete(mostRecent->second))
            {
               std::shared_ptr<rda::ElevationScan> secondMostRecent = nullptr;

               // check if this volume scan has an earlier elevation scan
               auto possibleSecondMostRecent = elevation.second.rbegin();
               ++possibleSecondMostRecent;

               if (possibleSecondMostRecent == elevation.second.rend())
               {
                  if (last == nullptr)
                  {
                     // Nothing to merge with
                     p->index_[type.first][elevation.first][mostRecent->first] =
                        mostRecent->second;
                     continue;
                  }

                  // get the scan from the last scan
                  auto elevationScan =
                     std::get<std::shared_ptr<rda::ElevationScan>>(
                        last->GetElevationScan(
                           type.first, elevation.first, {}));
                  if (elevationScan == nullptr)
                  {
                     // Nothing to merge with
                     p->index_[type.first][elevation.first][mostRecent->first] =
                        mostRecent->second;
                     continue;
                  }

                  secondMostRecent = elevationScan;
               }
               else
               {
                  secondMostRecent = possibleSecondMostRecent->second;
               }

               // Make the new scan
               auto newScan = std::make_shared<rda::ElevationScan>();

               // Copy over the new radials
               for (const auto& radial : *(mostRecent->second))
               {
                  (*newScan)[radial.first] = radial.second;
               }

               /* Correctly order the old radials. The radials need to be in
                * order for the rendering to work, and the index needs to start
                * at 0 and increase by one from there. Since the new radial
                * should have index 0, the old radial needs to be reshaped to
                * match the new radials indexing.
                */

               const double lowestAzm =
                  mostRecent->second->cbegin()->second->azimuth_angle().value();
               const double heighestAzm = mostRecent->second->crbegin()
                                             ->second->azimuth_angle()
                                             .value();
               std::uint16_t index = mostRecent->second->crbegin()->first + 1;

               // Sort by the azimuth. Makes the rest of this way easier
               auto secondMostRecentAzmMap =
                  std::map<float, std::shared_ptr<rda::GenericRadarData>>();
               for (const auto& radial : *secondMostRecent)
               {
                  secondMostRecentAzmMap[radial.second->azimuth_angle()
                                            .value()] = radial.second;
               }

               if (lowestAzm <= heighestAzm) // New scan does not contain 0/360
               {
                  // Get the radials following the new radials
                  for (const auto& radial : secondMostRecentAzmMap)
                  {
                     if (radial.first > heighestAzm)
                     {
                        (*newScan)[index] = radial.second;
                        ++index;
                     }
                  }
                  // Get the radials before the new radials
                  for (const auto& radial : secondMostRecentAzmMap)
                  {
                     if (radial.first < lowestAzm)
                     {
                        (*newScan)[index] = radial.second;
                        ++index;
                     }
                     else
                     {
                        break;
                     }
                  }
               }
               else // New scan includes 0/360
               {
                  // The radials will already be in the right order
                  for (const auto& radial : secondMostRecentAzmMap)
                  {
                     if (radial.first > heighestAzm && radial.first < lowestAzm)
                     {
                        (*newScan)[index] = radial.second;
                        ++index;
                     }
                  }
               }

               p->index_[type.first][elevation.first][mostRecent->first] =
                  newScan;
            }
            else
            {
               p->index_[type.first][elevation.first][mostRecent->first] =
                  mostRecent->second;
            }
         }
      }
   }

   // Go though last, adding other elevations
   if (last != nullptr)
   {
      for (const auto& type : last->p->index_)
      {
         // Find the highest elevation this type has for the current scan
         // Start below any reasonable elevation
         // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
         float       highestCurrentElevation = -90;
         const auto& elevationScans          = p->index_.find(type.first);
         if (elevationScans != p->index_.cend())
         {
            const auto& highestElevation = elevationScans->second.crbegin();
            if (highestElevation != elevationScans->second.crend())
            {
               // Add a slight offset to ensure good floating point compare.
               // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
               highestCurrentElevation = highestElevation->first + 0.01f;
            }
         }

         for (const auto& elevation : type.second)
         {
            // Only add elevations above the current scan's elevation
            if (elevation.first > highestCurrentElevation)
            {
               const auto& mostRecent = elevation.second.crbegin();
               if (mostRecent == elevation.second.crend())
               {
                  continue;
               }
               p->index_[type.first][elevation.first][mostRecent->first] =
                  mostRecent->second;
            }
         }
      }
   }
}

} // namespace wsr88d
} // namespace scwx
