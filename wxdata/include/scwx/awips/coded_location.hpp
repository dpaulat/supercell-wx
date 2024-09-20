#pragma once

#include <scwx/common/geographic.hpp>

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

class CodedLocationImpl;

class CodedLocation
{
public:
   typedef boost::any_range<std::string, boost::forward_traversal_tag>
      StringRange;

   explicit CodedLocation();
   ~CodedLocation();

   CodedLocation(const CodedLocation&) = delete;
   CodedLocation& operator=(const CodedLocation&) = delete;

   CodedLocation(CodedLocation&&) noexcept;
   CodedLocation& operator=(CodedLocation&&) noexcept;

   std::vector<common::Coordinate> coordinates() const;

   bool Parse(const StringRange& lines, const std::string& wfo = {});

   static std::optional<CodedLocation> Create(const StringRange& lines,
                                              const std::string& wfo = {});

private:
   std::unique_ptr<CodedLocationImpl> p;
};

} // namespace awips
} // namespace scwx
