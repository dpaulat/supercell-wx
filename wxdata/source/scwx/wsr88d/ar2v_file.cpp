#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/rda/level2_message_factory.hpp>
#include <scwx/wsr88d/rda/types.hpp>
#include <scwx/util/logger.hpp>
#include <scwx/util/rangebuf.hpp>
#include <scwx/util/time.hpp>

#include <fstream>
#include <sstream>

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable : 4702)
#endif

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

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
   explicit Ar2vFileImpl() :
       tapeFilename_ {},
       extensionNumber_ {},
       julianDate_ {0},
       milliseconds_ {0},
       icao_ {},
       vcpData_ {nullptr},
       radarData_ {},
       index_ {},
       rawRecords_ {} {};
   ~Ar2vFileImpl() = default;

   std::size_t DecompressLDMRecords(std::istream& is);
   void        HandleMessage(std::shared_ptr<rda::Level2Message>& message);
   void        IndexFile();
   void        ParseLDMRecords();
   void        ParseLDMRecord(std::istream& is);
   void        ProcessRadarData(
             const std::shared_ptr<rda::DigitalRadarDataGeneric>& message);

   std::string   tapeFilename_;
   std::string   extensionNumber_;
   std::uint32_t julianDate_;
   std::uint32_t milliseconds_;
   std::string   icao_;

   std::shared_ptr<rda::VolumeCoveragePatternData>              vcpData_;
   std::map<std::uint16_t, std::shared_ptr<rda::ElevationScan>> radarData_;

   std::map<rda::DataBlockType,
            std::map<std::uint16_t, std::shared_ptr<rda::ElevationScan>>>
      index_;

   std::list<std::stringstream> rawRecords_;
};

Ar2vFile::Ar2vFile() : p(std::make_unique<Ar2vFileImpl>()) {}
Ar2vFile::~Ar2vFile() = default;

Ar2vFile::Ar2vFile(Ar2vFile&&) noexcept            = default;
Ar2vFile& Ar2vFile::operator=(Ar2vFile&&) noexcept = default;

uint32_t Ar2vFile::julian_date() const
{
   return p->julianDate_;
}

uint32_t Ar2vFile::milliseconds() const
{
   return p->milliseconds_;
}

std::string Ar2vFile::icao() const
{
   return p->icao_;
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
      std::shared_ptr<rda::DigitalRadarDataGeneric> lastRadial =
         p->radarData_.crbegin()->second->crbegin()->second;

      endTime = util::TimePoint(lastRadial->modified_julian_date(),
                                lastRadial->collection_time());
   }

   return endTime;
}

std::map<uint16_t, std::shared_ptr<rda::ElevationScan>>
Ar2vFile::radar_data() const
{
   return p->radarData_;
}

std::shared_ptr<const rda::VolumeCoveragePatternData> Ar2vFile::vcp_data() const
{
   return p->vcpData_;
}

std::tuple<std::shared_ptr<rda::ElevationScan>, float, std::vector<float>>
Ar2vFile::GetElevationScan(rda::DataBlockType dataBlockType,
                           float              elevation,
                           std::chrono::system_clock::time_point /*time*/) const
{
   logger_->debug("GetElevationScan: {} degrees", elevation);

   constexpr float scaleFactor = 8.0f / 0.043945f;

   std::shared_ptr<rda::ElevationScan> elevationScan = nullptr;
   float                               elevationCut  = 0.0f;
   std::vector<float>                  elevationCuts;

   uint16_t codedElevation =
      static_cast<uint16_t>(std::lroundf(elevation * scaleFactor));

   if (p->index_.contains(dataBlockType))
   {
      auto scans = p->index_.at(dataBlockType);

      uint16_t lowerBound = scans.cbegin()->first;
      uint16_t upperBound = scans.crbegin()->first;

      for (auto scan : scans)
      {
         if (scan.first > lowerBound && scan.first <= codedElevation)
         {
            lowerBound = scan.first;
         }
         if (scan.first < upperBound && scan.first >= codedElevation)
         {
            upperBound = scan.first;
         }

         elevationCuts.push_back(scan.first / scaleFactor);
      }

      int32_t lowerDelta = std::abs(static_cast<int32_t>(codedElevation) -
                                    static_cast<int32_t>(lowerBound));
      int32_t upperDelta = std::abs(static_cast<int32_t>(codedElevation) -
                                    static_cast<int32_t>(upperBound));

      if (lowerDelta < upperDelta)
      {
         elevationScan = scans.at(lowerBound);
         elevationCut  = lowerBound / scaleFactor;
      }
      else
      {
         elevationScan = scans.at(upperBound);
         elevationCut  = upperBound / scaleFactor;
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

   if (dataValid)
   {
      logger_->debug("Filename:  {}", p->tapeFilename_);
      logger_->debug("Extension: {}", p->extensionNumber_);
      logger_->debug("Date:      {}", p->julianDate_);
      logger_->debug("Time:      {}", p->milliseconds_);
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

size_t Ar2vFileImpl::DecompressLDMRecords(std::istream& is)
{
   logger_->debug("Decompressing LDM Records");

   size_t numRecords = 0;

   while (is.peek() != EOF)
   {
      std::streampos startPosition = is.tellg();
      int32_t        controlWord   = 0;
      size_t         recordSize;

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
         std::stringstream ss;
         std::streamsize   bytesCopied = boost::iostreams::copy(in, ss);
         logger_->trace("Decompressed record size = {} bytes", bytesCopied);

         rawRecords_.push_back(std::move(ss));
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

   logger_->debug("Decompressed {} LDM Records", numRecords);

   return numRecords;
}

void Ar2vFileImpl::ParseLDMRecords()
{
   logger_->debug("Parsing LDM Records");

   size_t count = 0;

   for (auto it = rawRecords_.begin(); it != rawRecords_.end(); it++)
   {
      std::stringstream& ss = *it;

      logger_->trace("Record {}", count++);

      ParseLDMRecord(ss);
   }

   rawRecords_.clear();
}

void Ar2vFileImpl::ParseLDMRecord(std::istream& is)
{
   auto ctx = rda::Level2MessageFactory::CreateContext();

   // The communications manager inserts an extra 12 bytes at the beginning
   // of each record
   is.seekg(12, std::ios_base::cur);

   while (!is.eof())
   {
      off_t    offset   = 0;
      uint16_t nextSize = 0u;
      do
      {
         is.read(reinterpret_cast<char*>(&nextSize), 2);
         if (nextSize == 0)
         {
            offset += 2;
         }
         else
         {
            is.seekg(-2, std::ios_base::cur);
         }
      } while (!is.eof() && nextSize == 0u);

      if (!is.eof() && offset != 0)
      {
         logger_->trace("Next record offset by {} bytes", offset);
      }
      else if (is.eof())
      {
         break;
      }

      rda::Level2MessageInfo msgInfo =
         rda::Level2MessageFactory::Create(is, ctx);
      if (!msgInfo.headerValid)
      {
         // Invalid message
         break;
      }

      if (msgInfo.messageValid)
      {
         HandleMessage(msgInfo.message);
      }
   }
}

void Ar2vFileImpl::HandleMessage(std::shared_ptr<rda::Level2Message>& message)
{
   switch (message->header().message_type())
   {
   case static_cast<uint8_t>(rda::MessageId::VolumeCoveragePatternData):
      vcpData_ =
         std::static_pointer_cast<rda::VolumeCoveragePatternData>(message);
      break;

   case static_cast<uint8_t>(rda::MessageId::DigitalRadarData):
      ProcessRadarData(
         std::static_pointer_cast<rda::DigitalRadarDataGeneric>(message));
      break;

   default:
      break;
   }
}

void Ar2vFileImpl::ProcessRadarData(
   const std::shared_ptr<rda::DigitalRadarDataGeneric>& message)
{
   uint16_t azimuthIndex   = message->azimuth_number() - 1;
   uint16_t elevationIndex = message->elevation_number() - 1;

   if (radarData_[elevationIndex] == nullptr)
   {
      radarData_[elevationIndex] = std::make_shared<rda::ElevationScan>();
   }

   (*radarData_[elevationIndex])[azimuthIndex] = message;
}

void Ar2vFileImpl::IndexFile()
{
   logger_->debug("Indexing file");

   if (vcpData_ == nullptr)
   {
      logger_->warn("Cannot index file without VCP data");
      return;
   }

   for (auto& elevationCut : radarData_)
   {
      uint16_t elevationAngle =
         vcpData_->elevation_angle_raw(elevationCut.first);
      rda::WaveformType waveformType =
         vcpData_->waveform_type(elevationCut.first);

      std::shared_ptr<rda::DigitalRadarDataGeneric>& radial0 =
         (*elevationCut.second)[0];

      if (radial0 == nullptr)
      {
         logger_->warn("Empty radial data");
         continue;
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
            // TODO: Handle multiple elevation scans
            index_[dataBlockType][elevationAngle] = elevationCut.second;
         }
      }
   }
}

} // namespace wsr88d
} // namespace scwx
