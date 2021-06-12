#include <scwx/wsr88d/rpg/ar2v_file.hpp>
#include <scwx/util/rangebuf.hpp>

#include <fstream>
#include <sstream>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/log/trivial.hpp>

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

static const std::string logPrefix_ = "[scwx::wsr88d::rpg::ar2v_file] ";

class Ar2vFileImpl
{
public:
   explicit Ar2vFileImpl() :
       tapeFilename_(),
       extensionNumber_(),
       julianDate_ {0},
       milliseconds_ {0},
       icao_(),
       numRecords_ {0} {};
   ~Ar2vFileImpl() = default;

   void ParseLDMRecords(std::ifstream& f);

   std::string tapeFilename_;
   std::string extensionNumber_;
   int32_t     julianDate_;
   int32_t     milliseconds_;
   std::string icao_;

   size_t numRecords_;
};

Ar2vFile::Ar2vFile() : p(std::make_unique<Ar2vFileImpl>()) {}
Ar2vFile::~Ar2vFile() = default;

Ar2vFile::Ar2vFile(Ar2vFile&&) noexcept = default;
Ar2vFile& Ar2vFile::operator=(Ar2vFile&&) = default;

bool Ar2vFile::LoadFile(const std::string& filename)
{
   BOOST_LOG_TRIVIAL(debug) << logPrefix_ << "LoadFile(" << filename << ")\n";
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

      p->julianDate_   = htonl(p->julianDate_);
      p->milliseconds_ = htonl(p->milliseconds_);
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

      p->ParseLDMRecords(f);
   }

   return fileValid;
}

void Ar2vFileImpl::ParseLDMRecords(std::ifstream& f)
{
   numRecords_ = 0;

   while (f.peek() != EOF)
   {
      std::streampos startPosition = f.tellg();
      int32_t        controlWord   = 0;
      size_t         recordSize;

      f.read(reinterpret_cast<char*>(&controlWord), 4);

      controlWord = htonl(controlWord);
      recordSize  = std::abs(controlWord);

      BOOST_LOG_TRIVIAL(debug)
         << logPrefix_ << "LDM Record Found: Size = " << recordSize << " bytes";

      boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
      util::rangebuf r(f.rdbuf(), recordSize);
      in.push(boost::iostreams::bzip2_decompressor());
      in.push(r);

      std::ostringstream of;

      try
      {
         std::streamsize bytesCopied = boost::iostreams::copy(in, of);
         BOOST_LOG_TRIVIAL(debug)
            << logPrefix_ << "Decompressed record size = " << bytesCopied
            << " bytes";
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

   BOOST_LOG_TRIVIAL(debug)
      << logPrefix_ << "Found " << numRecords_ << " LDM Records";
}

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
