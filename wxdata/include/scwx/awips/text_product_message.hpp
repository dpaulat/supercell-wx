#pragma once

#include <scwx/awips/message.hpp>

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

   size_t data_size() const;

   bool Parse(std::istream& is) override;

   static std::shared_ptr<TextProductMessage> Create(std::istream& is);

private:
   std::unique_ptr<TextProductMessageImpl> p;
};

} // namespace awips
} // namespace scwx
