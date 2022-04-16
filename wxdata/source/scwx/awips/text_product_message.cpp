#include <scwx/awips/text_product_message.hpp>
#include <scwx/awips/coded_location.hpp>
#include <scwx/awips/coded_time_motion_location.hpp>
#include <scwx/awips/pvtec.hpp>
#include <scwx/common/characters.hpp>
#include <scwx/util/streams.hpp>

#include <istream>
#include <regex>
#include <string>

namespace scwx
{
namespace awips
{

static const std::string logPrefix_ = "scwx::awips::text_product_message";

// Issuance date/time takes one of the following forms:
// * <hhmm>_xM_<tz>_day_mon_<dd>_year
// * <hhmm>_UTC_day_mon_<dd>_year
// Segment Header only:
// * <hhmm>_xM_<tz1>_day_mon_<dd>_year_/<hhmm>_xM_<tz2>_day_mon_<dd>_year/
// Look for hhmm (xM|UTC) to key the date/time string
static const std::regex reDateTimeString {"^[0-9]{3,4} ([AP]M|UTC)"};

struct Vtec
{
   PVtec       pVtec_;
   std::string hVtec_;

   Vtec() : pVtec_ {}, hVtec_ {} {}

   Vtec(const Vtec&) = delete;
   Vtec& operator=(const Vtec&) = delete;

   Vtec(Vtec&&) noexcept = default;
   Vtec& operator=(Vtec&&) noexcept = default;
};

struct SegmentHeader
{
   std::string              ugcString_;
   std::vector<Vtec>        vtecString_;
   std::vector<std::string> ugcNames_;
   std::string              issuanceDateTime_;

   SegmentHeader() :
       ugcString_ {}, vtecString_ {}, ugcNames_ {}, issuanceDateTime_ {}
   {
   }

   SegmentHeader(const SegmentHeader&) = delete;
   SegmentHeader& operator=(const SegmentHeader&) = delete;

   SegmentHeader(SegmentHeader&&) noexcept = default;
   SegmentHeader& operator=(SegmentHeader&&) noexcept = default;
};

struct Segment
{
   std::optional<SegmentHeader>           header_;
   std::vector<std::string>               productContent_;
   std::optional<CodedLocation>           codedLocation_;
   std::optional<CodedTimeMotionLocation> codedMotion_;

   Segment() :
       header_ {}, productContent_ {}, codedLocation_ {}, codedMotion_ {}
   {
   }

   Segment(const Segment&) = delete;
   Segment& operator=(const Segment&) = delete;

   Segment(Segment&&) noexcept = default;
   Segment& operator=(Segment&&) noexcept = default;
};

static void ParseCodedInformation(std::shared_ptr<Segment> segment,
                                  const std::string&       wfo);
static std::vector<std::string>     ParseProductContent(std::istream& is);
static void                         SkipBlankLines(std::istream& is);
static bool                         TryParseEndOfProduct(std::istream& is);
static std::vector<std::string>     TryParseMndHeader(std::istream& is);
static std::vector<std::string>     TryParseOverviewBlock(std::istream& is);
static std::optional<SegmentHeader> TryParseSegmentHeader(std::istream& is);
static std::optional<Vtec>          TryParseVtecString(std::istream& is);

class TextProductMessageImpl
{
public:
   explicit TextProductMessageImpl() : wmoHeader_ {} {}
   ~TextProductMessageImpl() = default;

   std::shared_ptr<WmoHeader>            wmoHeader_;
   std::vector<std::string>              mndHeader_;
   std::vector<std::string>              overviewBlock_;
   std::vector<std::shared_ptr<Segment>> segments_;
};

TextProductMessage::TextProductMessage() :
    p(std::make_unique<TextProductMessageImpl>())
{
}
TextProductMessage::~TextProductMessage() = default;

TextProductMessage::TextProductMessage(TextProductMessage&&) noexcept = default;
TextProductMessage&
TextProductMessage::operator=(TextProductMessage&&) noexcept = default;

std::shared_ptr<WmoHeader> TextProductMessage::wmo_header() const
{
   return p->wmoHeader_;
}

size_t TextProductMessage::data_size() const
{
   return 0;
}

bool TextProductMessage::Parse(std::istream& is)
{
   bool dataValid = true;

   p->wmoHeader_ = std::make_shared<WmoHeader>();
   dataValid     = p->wmoHeader_->Parse(is);

   for (size_t i = 0; dataValid && !is.eof(); i++)
   {
      if (i != 0 && TryParseEndOfProduct(is))
      {
         break;
      }

      std::shared_ptr<Segment> segment = std::make_shared<Segment>();

      if (i == 0)
      {
         if (is.peek() != '\r')
         {
            segment->header_ = TryParseSegmentHeader(is);
         }

         SkipBlankLines(is);

         p->mndHeader_ = TryParseMndHeader(is);
         SkipBlankLines(is);

         // Optional overview block appears between MND and segment header
         if (!segment->header_.has_value())
         {
            p->overviewBlock_ = TryParseOverviewBlock(is);
            SkipBlankLines(is);
         }
      }

      if (!segment->header_.has_value())
      {
         segment->header_ = TryParseSegmentHeader(is);
         SkipBlankLines(is);
      }

      segment->productContent_ = ParseProductContent(is);
      SkipBlankLines(is);

      ParseCodedInformation(segment, p->wmoHeader_->icao());

      if (segment->header_.has_value() || !segment->productContent_.empty())
      {
         p->segments_.push_back(std::move(segment));
      }
   }

   return dataValid;
}

void ParseCodedInformation(std::shared_ptr<Segment> segment,
                           const std::string&       wfo)
{
   typedef std::vector<std::string>::const_iterator StringIterator;

   std::vector<std::string>& productContent = segment->productContent_;

   StringIterator codedLocationBegin = productContent.cend();
   StringIterator codedLocationEnd   = productContent.cend();
   StringIterator codedMotionBegin   = productContent.cend();
   StringIterator codedMotionEnd     = productContent.cend();

   for (auto it = productContent.cbegin(); it != productContent.cend(); ++it)
   {
      if (codedLocationBegin == productContent.cend() &&
          it->starts_with("LAT...LON"))
      {
         codedLocationBegin = it;
      }
      else if (codedLocationBegin != productContent.cend() &&
               codedLocationEnd == productContent.cend() &&
               !it->starts_with(" ") /* Continuation line */)
      {
         codedLocationEnd = it;
      }

      if (codedMotionBegin == productContent.cend() &&
          it->starts_with("TIME...MOT...LOC"))
      {
         codedMotionBegin = it;
      }
      else if (codedMotionBegin != productContent.cend() &&
               codedMotionEnd == productContent.cend() &&
               !it->starts_with(" ") /* Continuation line */)
      {
         codedMotionEnd = it;
      }
   }

   if (codedLocationBegin != productContent.cend())
   {
      segment->codedLocation_ =
         CodedLocation::Create({codedLocationBegin, codedLocationEnd}, wfo);
   }

   if (codedMotionBegin != productContent.cend())
   {
      segment->codedMotion_ = CodedTimeMotionLocation::Create(
         {codedMotionBegin, codedMotionEnd}, wfo);
   }
}

std::vector<std::string> ParseProductContent(std::istream& is)
{
   std::vector<std::string> productContent;
   std::string              line;

   while (!is.eof() && is.peek() != common::Characters::ETX)
   {
      util::getline(is, line);

      if (!productContent.empty() || !line.starts_with("$$"))
      {
         productContent.push_back(line);
      }

      if (line.starts_with("$$"))
      {
         // End of Product or Product Segment Code
         break;
      }
   }

   while (!productContent.empty() && productContent.back().empty())
   {
      productContent.pop_back();
   }

   return productContent;
}

void SkipBlankLines(std::istream& is)
{
   std::string line;

   while (is.peek() == '\r')
   {
      util::getline(is, line);
   }
}

bool TryParseEndOfProduct(std::istream& is)
{
   std::string    line;
   std::streampos isBegin     = is.tellg();
   bool           endOfStream = false;

   if (is.peek() == common::Characters::ETX)
   {
      is.get();
      endOfStream = true;
   }
   else if (is.peek() == EOF)
   {
      endOfStream = true;
   }

   if (!endOfStream)
   {
      // Optional Forecast Identifier
      util::getline(is, line);
      SkipBlankLines(is);

      if (is.peek() == common::Characters::ETX)
      {
         is.get();
         endOfStream = true;
      }
      else if (is.peek() == EOF)
      {
         endOfStream = true;
      }
   }

   if (!endOfStream)
   {
      // End of Product was not found, so reset the istream to the original
      // state
      is.seekg(isBegin, std::ios_base::beg);
   }

   return endOfStream;
}

std::vector<std::string> TryParseMndHeader(std::istream& is)
{
   std::vector<std::string> mndHeader;
   std::string              line;
   std::streampos           isBegin = is.tellg();

   while (!is.eof() && is.peek() != '\r')
   {
      util::getline(is, line);
      mndHeader.push_back(line);
   }

   if (!mndHeader.empty() &&
       !std::regex_search(mndHeader.back(), reDateTimeString))
   {
      // MND Header should end with an Issuance Date/Time Line
      mndHeader.clear();
   }

   if (mndHeader.empty())
   {
      // MND header was not found, so reset the istream to the original state
      is.seekg(isBegin, std::ios_base::beg);
   }

   return mndHeader;
}

std::vector<std::string> TryParseOverviewBlock(std::istream& is)
{
   // Optional overview block contains text in the following format:
   // ...OVERVIEW HEADLINE... /OPTIONAL/
   // .OVERVIEW WITH GENERAL INFORMATION / OPTIONAL /
   // Key off the block beginning with .
   std::vector<std::string> overviewBlock;
   std::string              line;

   if (is.peek() == '.')
   {
      while (!is.eof() && is.peek() != '\r')
      {
         util::getline(is, line);
         overviewBlock.push_back(line);
      }
   }

   return overviewBlock;
}

std::optional<SegmentHeader> TryParseSegmentHeader(std::istream& is)
{
   // UGC takes the form SSFNNN-NNN>NNN-SSFNNN-DDHHMM- (NWSI 10-1702)
   // Look for SSF(NNN)?[->] to key the UGC string
   static const std::regex reUgcString {"^[A-Z]{2}[CZ]([0-9]{3})?[->]"};

   std::optional<SegmentHeader> header = std::nullopt;
   std::string                  line;
   std::streampos               isBegin = is.tellg();

   util::getline(is, line);

   if (std::regex_search(line, reUgcString))
   {
      header = SegmentHeader();
      header->ugcString_.swap(line);
   }

   if (header.has_value())
   {
      std::optional<Vtec> vtec;
      while ((vtec = TryParseVtecString(is)) != std::nullopt)
      {
         header->vtecString_.push_back(std::move(*vtec));
      }

      while (!is.eof() && is.peek() != '\r')
      {
         util::getline(is, line);
         if (!std::regex_search(line, reDateTimeString))
         {
            header->ugcNames_.push_back(line);
         }
         else
         {
            header->issuanceDateTime_.swap(line);
            break;
         }
      }
   }

   if (!header.has_value())
   {
      // We did not find a valid segment header, so we reset the istream to the
      // original state
      is.seekg(isBegin, std::ios_base::beg);
   }

   return header;
}

std::optional<Vtec> TryParseVtecString(std::istream& is)
{
   // P-VTEC takes the form /k.aaa.cccc.pp.s.####.yymmddThhnnZB-yymmddThhnnZE/
   // (NWSI 10-1703)
   // Look for /k. to key the P-VTEC string
   static const std::regex rePVtecString {"^/[OTEX]\\."};

   // H-VTEC takes the form
   // /nwsli.s.ic.yymmddThhnnZB.yymmddThhnnZC.yymmddThhnnZE.fr/ (NWSI 10-1703)
   // Look for /nwsli. to key the H-VTEC string
   static const std::regex reHVtecString {"^/[A-Z0-9]{5}\\."};

   std::optional<Vtec> vtec = std::nullopt;
   std::string         line;
   std::streampos      isBegin = is.tellg();

   util::getline(is, line);

   if (std::regex_search(line, rePVtecString))
   {
      bool vtecValid;

      vtec      = Vtec();
      vtecValid = vtec->pVtec_.Parse(line);

      isBegin = is.tellg();

      util::getline(is, line);

      if (std::regex_search(line, reHVtecString))
      {
         vtec->hVtec_.swap(line);
      }
      else
      {
         // H-VTEC was not found, so reset the istream to the beginning of
         // the line
         is.seekg(isBegin, std::ios_base::beg);
      }

      if (!vtecValid)
      {
         vtec.reset();
      }
   }
   else
   {
      // P-VTEC was not found, so reset the istream to the original state
      is.seekg(isBegin, std::ios_base::beg);
   }

   return vtec;
}

std::shared_ptr<TextProductMessage> TextProductMessage::Create(std::istream& is)
{
   std::shared_ptr<TextProductMessage> message =
      std::make_shared<TextProductMessage>();

   if (!message->Parse(is))
   {
      message.reset();
   }

   return message;
}

} // namespace awips
} // namespace scwx
