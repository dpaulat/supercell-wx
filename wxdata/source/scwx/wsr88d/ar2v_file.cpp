#include <scwx/wsr88d/ar2v_file.hpp>
#include <scwx/wsr88d/rda/message_factory.hpp>
#include <scwx/wsr88d/rda/types.hpp>
#include <scwx/util/rangebuf.hpp>
#include <scwx/util/time.hpp>

#include <fstream>
#include <sstream>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/log/trivial.hpp>

namespace scwx
{
namespace wsr88d
{

static const std::string logPrefix_ = "[scwx::wsr88d::ar2v_file] ";

class Ar2vFileImpl
{
public:
   explicit Ar2vFileImpl() :
       tapeFilename_(),
       extensionNumber_(),
       julianDate_ {0},
       milliseconds_ {0},
       icao_(),
       numRecords_ {0},
       rawRecords_(),
       vcpData_ {nullptr},
       radarData_ {} {};
   ~Ar2vFileImpl() = default;

   void HandleMessage(std::shared_ptr<rda::Message>& message);
   void LoadLDMRecords(std::ifstream& f);
   void ParseLDMRecords();
   void ProcessRadarData(std::shared_ptr<rda::DigitalRadarData> message);

   std::string tapeFilename_;
   std::string extensionNumber_;
   uint32_t    julianDate_;
   uint32_t    milliseconds_;
   std::string icao_;

   size_t numRecords_;

   std::shared_ptr<rda::VolumeCoveragePatternData>         vcpData_;
   std::map<uint16_t, std::shared_ptr<rda::ElevationScan>> radarData_;

   std::list<std::stringstream> rawRecords_;
};

Ar2vFile::Ar2vFile() : p(std::make_unique<Ar2vFileImpl>()) {}
Ar2vFile::~Ar2vFile() = default;

Ar2vFile::Ar2vFile(Ar2vFile&&) noexcept = default;
Ar2vFile& Ar2vFile::operator=(Ar2vFile&&) noexcept = default;

uint32_t Ar2vFile::julian_date() const
{
   return p->julianDate_;
}

uint32_t Ar2vFile::milliseconds() const
{
   return p->milliseconds_;
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
      std::shared_ptr<rda::DigitalRadarData> lastRadial =
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

bool Ar2vFile::LoadFile(const std::string& filename)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "LoadFile(" << filename << ")";
   bool fileValid = true;

   std::ifstream f(filename, std::ios_base::in | std::ios_base::binary);
   if (!f.good())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Could not open file for reading: " << filename;
      fileValid = false;
   }

   if (fileValid)
   {
      // Read Volume Header Record
      p->tapeFilename_.resize(9, ' ');
      p->extensionNumber_.resize(3, ' ');
      p->icao_.resize(4, ' ');

      f.read(&p->tapeFilename_[0], 9);
      f.read(&p->extensionNumber_[0], 3);
      f.read(reinterpret_cast<char*>(&p->julianDate_), 4);
      f.read(reinterpret_cast<char*>(&p->milliseconds_), 4);
      f.read(&p->icao_[0], 4);

      p->julianDate_   = ntohl(p->julianDate_);
      p->milliseconds_ = ntohl(p->milliseconds_);
   }

   if (f.eof())
   {
      BOOST_LOG_TRIVIAL(warning)
         << logPrefix_ << "Could not read Volume Header Record\n";
      fileValid = false;
   }

   if (fileValid)
   {
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Filename:  " << p->tapeFilename_;
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Extension: " << p->extensionNumber_;
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Date:      " << p->julianDate_;
      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "Time:      " << p->milliseconds_;
      BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "ICAO:      " << p->icao_;

      p->LoadLDMRecords(f);
   }

   return fileValid;
}

void Ar2vFileImpl::LoadLDMRecords(std::ifstream& f)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Loading LDM Records";

   numRecords_ = 0;

   while (f.peek() != EOF)
   {
      std::streampos startPosition = f.tellg();
      int32_t        controlWord   = 0;
      size_t         recordSize;

      f.read(reinterpret_cast<char*>(&controlWord), 4);

      controlWord = ntohl(controlWord);
      recordSize  = std::abs(controlWord);

      BOOST_LOG_TRIVIAL(trace)
         << logPrefix_ << "LDM Record Found: Size = " << recordSize << " bytes";

      boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
      util::rangebuf r(f.rdbuf(), recordSize);
      in.push(boost::iostreams::bzip2_decompressor());
      in.push(r);

      try
      {
         std::stringstream ss;
         std::streamsize   bytesCopied = boost::iostreams::copy(in, ss);
         BOOST_LOG_TRIVIAL(trace)
            << logPrefix_ << "Decompressed record size = " << bytesCopied
            << " bytes";

         rawRecords_.push_back(std::move(ss));
      }
      catch (const boost::iostreams::bzip2_error& ex)
      {
         int error = ex.error();
         BOOST_LOG_TRIVIAL(warning)
            << logPrefix_ << "Error decompressing record " << numRecords_;

         f.seekg(startPosition + std::streampos(recordSize));
      }

      ++numRecords_;
   }

   ParseLDMRecords();

   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Found " << numRecords_ << " LDM Records";
}

void Ar2vFileImpl::ParseLDMRecords()
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "Parsing LDM Records";

   size_t count = 0;

   for (auto it = rawRecords_.begin(); it != rawRecords_.end(); it++)
   {
      std::stringstream& ss = *it;

      BOOST_LOG_TRIVIAL(trace) << logPrefix_ << "Record " << count++;

      // The communications manager inserts an extra 12 bytes at the beginning
      // of each record
      ss.seekg(12);

      while (!ss.eof())
      {
         rda::MessageInfo msgInfo = rda::MessageFactory::Create(ss);
         if (!msgInfo.headerValid)
         {
            // Invalid message
            break;
         }

         if (msgInfo.messageValid)
         {
            HandleMessage(msgInfo.message);
         }

         off_t    offset   = 0;
         uint16_t nextSize = 0u;
         do
         {
            ss.read(reinterpret_cast<char*>(&nextSize), 2);
            if (nextSize == 0)
            {
               offset += 2;
            }
            else
            {
               ss.seekg(-2, std::ios_base::cur);
            }
         } while (!ss.eof() && nextSize == 0u);

         if (!ss.eof() && offset != 0)
         {
            BOOST_LOG_TRIVIAL(trace)
               << logPrefix_ << "Next record offset by " << offset << " bytes";
         }
      }
   }

   rawRecords_.clear();
}

void Ar2vFileImpl::HandleMessage(std::shared_ptr<rda::Message>& message)
{
   switch (message->header().message_type())
   {
   case static_cast<uint8_t>(rda::MessageId::VolumeCoveragePatternData):
      vcpData_ =
         std::static_pointer_cast<rda::VolumeCoveragePatternData>(message);
      break;

   case static_cast<uint8_t>(rda::MessageId::DigitalRadarData):
      ProcessRadarData(
         std::static_pointer_cast<rda::DigitalRadarData>(message));
      break;

   default: break;
   }
}

void Ar2vFileImpl::ProcessRadarData(
   std::shared_ptr<rda::DigitalRadarData> message)
{
   uint16_t azimuthIndex   = message->azimuth_number() - 1;
   uint16_t elevationIndex = message->elevation_number() - 1;

   if (radarData_[elevationIndex] == nullptr)
   {
      radarData_[elevationIndex] = std::make_shared<rda::ElevationScan>();
   }

   (*radarData_[elevationIndex])[azimuthIndex] = message;
}

} // namespace wsr88d
} // namespace scwx
