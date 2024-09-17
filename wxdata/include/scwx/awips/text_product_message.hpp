#pragma once

#include <scwx/awips/coded_location.hpp>
#include <scwx/awips/coded_time_motion_location.hpp>
#include <scwx/awips/impact_based_warnings.hpp>
#include <scwx/awips/message.hpp>
#include <scwx/awips/pvtec.hpp>
#include <scwx/awips/ugc.hpp>
#include <scwx/awips/wmo_header.hpp>

#include <chrono>
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
   std::vector<std::string> ugcString_;
   Ugc                      ugc_;
   std::vector<Vtec>        vtecString_;
   std::vector<std::string> ugcNames_;
   std::string              issuanceDateTime_;

   SegmentHeader() :
       ugcString_ {},
       ugc_ {},
       vtecString_ {},
       ugcNames_ {},
       issuanceDateTime_ {}
   {
   }

   SegmentHeader(const SegmentHeader&)            = delete;
   SegmentHeader& operator=(const SegmentHeader&) = delete;

   SegmentHeader(SegmentHeader&&) noexcept            = default;
   SegmentHeader& operator=(SegmentHeader&&) noexcept = default;
};

struct Segment
{
   std::shared_ptr<WmoHeader>             wmoHeader_ {};
   std::optional<SegmentHeader>           header_ {};
   std::vector<std::string>               productContent_ {};
   std::optional<CodedLocation>           codedLocation_ {};
   std::optional<CodedTimeMotionLocation> codedMotion_ {};

   bool                observed_ {false};
   ibw::ThreatCategory threatCategory_ {ibw::ThreatCategory::Base};
   bool                tornadoPossible_ {false};

   Segment() = default;

   Segment(const Segment&)            = delete;
   Segment& operator=(const Segment&) = delete;

   Segment(Segment&&) noexcept            = default;
   Segment& operator=(Segment&&) noexcept = default;

   std::chrono::system_clock::time_point event_begin() const;
   std::chrono::system_clock::time_point event_end() const;
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

   std::string                                 message_content() const;
   std::shared_ptr<WmoHeader>                  wmo_header() const;
   std::vector<std::string>                    mnd_header() const;
   std::vector<std::string>                    overview_block() const;
   std::size_t                                 segment_count() const;
   std::vector<std::shared_ptr<const Segment>> segments() const;
   std::shared_ptr<const Segment>              segment(std::size_t s) const;

   std::chrono::system_clock::time_point
   segment_event_begin(std::size_t s) const;

   std::size_t data_size() const override;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<TextProductMessage> Create(std::istream& is);

private:
   std::unique_ptr<TextProductMessageImpl> p;
};

} // namespace awips
} // namespace scwx
