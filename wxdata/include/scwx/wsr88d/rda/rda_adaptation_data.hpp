#pragma once

#include <scwx/wsr88d/rda/message.hpp>

#include <string>

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class RdaAdaptationDataImpl;

class RdaAdaptationData : public Message
{
public:
   explicit RdaAdaptationData();
   ~RdaAdaptationData();

   RdaAdaptationData(const Message&) = delete;
   RdaAdaptationData& operator=(const RdaAdaptationData&) = delete;

   RdaAdaptationData(RdaAdaptationData&&) noexcept;
   RdaAdaptationData& operator=(RdaAdaptationData&&) noexcept;

   const std::string& adap_file_name() const;
   const std::string& adap_format() const;

   bool Parse(std::istream& is);

   static std::unique_ptr<RdaAdaptationData> Create(MessageHeader&& header,
                                                    std::istream&   is);

private:
   std::unique_ptr<RdaAdaptationDataImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
