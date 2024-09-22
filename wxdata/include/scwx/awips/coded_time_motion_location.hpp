#pragma once

#include <scwx/common/geographic.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include <boost/range/any_range.hpp>

#if defined(__clang__)
#   pragma clang diagnostic pop
#endif

namespace scwx
{
namespace awips
{

class CodedTimeMotionLocationImpl;

class CodedTimeMotionLocation
{
public:
   typedef boost::any_range<std::string, boost::forward_traversal_tag>
      StringRange;

   explicit CodedTimeMotionLocation();
   ~CodedTimeMotionLocation();

   CodedTimeMotionLocation(const CodedTimeMotionLocation&) = delete;
   CodedTimeMotionLocation& operator=(const CodedTimeMotionLocation&) = delete;

   CodedTimeMotionLocation(CodedTimeMotionLocation&&) noexcept;
   CodedTimeMotionLocation& operator=(CodedTimeMotionLocation&&) noexcept;

   std::chrono::hh_mm_ss<std::chrono::minutes> time() const;
   uint16_t                                    direction() const;
   uint8_t                                     speed() const;
   std::vector<common::Coordinate>             coordinates() const;

   bool Parse(const StringRange& lines, const std::string& wfo = {});

   static std::optional<CodedTimeMotionLocation>
   Create(const StringRange& lines, const std::string& wfo = {});

private:
   std::unique_ptr<CodedTimeMotionLocationImpl> p;
};

} // namespace awips
} // namespace scwx
