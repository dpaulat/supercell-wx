#pragma once

#include <scwx/awips/coded_location.hpp>
#include <scwx/awips/coded_time_motion_location.hpp>
#include <scwx/awips/pvtec.hpp>
#include <scwx/awips/message.hpp>
#include <scwx/awips/wmo_header.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace scwx
{
namespace awips
{

struct Vtec
{
   PVtec       pVtec_;
   std::string hVtec_;

   Vtec() : pVtec_ {}, hVtec_ {} {}

   Vtec(const Vtec&)            = delete;
   Vtec& operator=(const Vtec&) = delete;

   Vtec(Vtec&&) noexcept            = default;
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

   SegmentHeader(const SegmentHeader&)            = delete;
   SegmentHeader& operator=(const SegmentHeader&) = delete;

   SegmentHeader(SegmentHeader&&) noexcept            = default;
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

   Segment(const Segment&)            = delete;
   Segment& operator=(const Segment&) = delete;

   Segment(Segment&&) noexcept            = default;
   Segment& operator=(Segment&&) noexcept = default;
};

class TextProductMessageImpl;

class TextProductMessage : public Message
{
public:
   explicit TextProductMessage();
   ~TextProductMessage();

   TextProductMessage(const TextProductMessage&)            = delete;
   TextProductMessage& operator=(const TextProductMessage&) = delete;

   TextProductMessage(TextProductMessage&&) noexcept;
   TextProductMessage& operator=(TextProductMessage&&) noexcept;

   std::shared_ptr<WmoHeader>                  wmo_header() const;
   std::vector<std::string>                    mnd_header() const;
   std::vector<std::string>                    overview_block() const;
   size_t                                      segment_count() const;
   std::vector<std::shared_ptr<const Segment>> segments() const;
   std::shared_ptr<const Segment>              segment(size_t s) const;

   size_t data_size() const;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<TextProductMessage> Create(std::istream& is);

private:
   std::unique_ptr<TextProductMessageImpl> p;
};

} // namespace awips
} // namespace scwx
