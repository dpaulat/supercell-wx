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

public:
   virtual ~Message();

   const MessageHeader& header() const;

   void set_header(MessageHeader&& header);

   virtual bool Parse(std::istream& is) = 0;

   static constexpr double ANGLE_DATA_SCALE      = 0.005493125;
   static constexpr double AZ_EL_RATE_DATA_SCALE = 0.001373291015625;

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
   static void SwapArray(std::array<float, _Size>& arr, size_t size = _Size)
   {
      std::transform(std::execution::par_unseq,
                     arr.begin(),
                     arr.begin() + size,
                     arr.begin(),
                     [](float f) { return SwapFloat(f); });
   }

   template<size_t _Size>
   static void SwapArray(std::array<uint16_t, _Size>& arr, size_t size = _Size)
   {
      std::transform(std::execution::par_unseq,
                     arr.begin(),
                     arr.begin() + size,
                     arr.begin(),
                     [](uint16_t u) { return ntohs(u); });
   }

   template<size_t _Size>
   static void SwapArray(std::array<uint32_t, _Size>& arr, size_t size = _Size)
   {
      std::transform(std::execution::par_unseq,
                     arr.begin(),
                     arr.begin() + size,
                     arr.begin(),
                     [](uint32_t u) { return ntohl(u); });
   }

   template<typename T>
   static void SwapMap(std::map<T, float>& m)
   {
      std::for_each(std::execution::par_unseq, m.begin(), m.end(), [](auto& p) {
         p.second = SwapFloat(p.second);
      });
   }

   static void SwapVector(std::vector<uint16_t>& v)
   {
      std::transform(std::execution::par_unseq,
                     v.begin(),
                     v.end(),
                     v.begin(),
                     [](uint16_t u) { return ntohs(u); });
   }

private:
   std::unique_ptr<MessageImpl> p;
};

} // namespace rda
} // namespace wsr88d
} // namespace scwx
