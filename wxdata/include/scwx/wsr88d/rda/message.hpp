#pragma once

#include <scwx/wsr88d/rda/message_header.hpp>

#include <array>
#include <execution>
#include <istream>
#include <map>
#include <string>

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx
{
namespace wsr88d
{
namespace rda
{

class MessageImpl;

class Message
{
protected:
   explicit Message();

   Message(const Message&) = delete;
   Message& operator=(const Message&) = delete;

   Message(Message&&) noexcept;
   Message& operator=(Message&&) noexcept;

   bool ValidateMessage(std::istream& is, size_t bytesRead) const;

   static void ReadBoolean(std::istream& is, bool& value)
   {
      std::string data(4, ' ');
      is.read(reinterpret_cast<char*>(&data[0]), 4);
      value = (data.at(0) == 'T');
   }

   static void ReadChar(std::istream& is, char& value)
   {
      std::string data(4, ' ');
      is.read(reinterpret_cast<char*>(&data[0]), 4);
      value = data.at(0);
   }

   static float SwapFloat(float f)
   {
      return ntohf(*reinterpret_cast<uint32_t*>(&f));
   }

   template<size_t _Size>
   static void SwapFloatArray(std::array<float, _Size>& arr)
   {
      std::transform(std::execution::par_unseq,
                     arr.begin(),
                     arr.end(),
                     arr.begin(),
                     [](float f) { return SwapFloat(f); });
   }

   template<size_t _Size>
   static void SwapUInt16Array(std::array<uint16_t, _Size>& arr)
   {
      std::transform(std::execution::par_unseq,
                     arr.begin(),
                     arr.end(),
                     arr.begin(),
                     [](uint16_t u) { return ntohs(u); });
   }

   template<typename T>
   static void SwapFloatMap(std::map<T, float>& m)
   {
      std::for_each(std::execution::par_unseq, m.begin(), m.end(), [](auto& p) {
         p.second = SwapFloat(p.second);
      });
   }

public:
   virtual ~Message();

   const MessageHeader& header() const;

   void set_header(MessageHeader&& header);

   virtual bool Parse(std::istream& is) = 0;

private:
   std::unique_ptr<MessageImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
