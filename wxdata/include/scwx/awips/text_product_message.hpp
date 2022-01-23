#pragma once

#include <scwx/awips/message.hpp>
#include <scwx/awips/wmo_header.hpp>

#include <cstdint>
#include <memory>

namespace scwx
{
namespace awips
{

class TextProductMessageImpl;

class TextProductMessage : public Message
{
public:
   explicit TextProductMessage();
   ~TextProductMessage();

   TextProductMessage(const TextProductMessage&) = delete;
   TextProductMessage& operator=(const TextProductMessage&) = delete;

   TextProductMessage(TextProductMessage&&) noexcept;
   TextProductMessage& operator=(TextProductMessage&&) noexcept;

   std::shared_ptr<WmoHeader> wmo_header() const;

   size_t data_size() const;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<TextProductMessage> Create(std::istream& is);

private:
   std::unique_ptr<TextProductMessageImpl> p;
};

} // namespace awips
} // namespace scwx
