#include <scwx/util/float.hpp>

#include <cmath>

#ifdef WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx
{
namespace util
{

float DecodeFloat16(uint16_t hex)
{
   static constexpr uint16_t S_MASK  = 0x8000;
   static constexpr uint16_t S_LSB   = 0;
   static constexpr uint16_t S_SHIFT = 15 - S_LSB;
   static constexpr uint16_t E_MASK  = 0x7a00;
   static constexpr uint16_t E_LSB   = 5;
   static constexpr uint16_t E_SHIFT = 15 - E_LSB;
   static constexpr uint16_t F_MASK  = 0x03ff;
   static constexpr uint16_t F_LSB   = 15;
   static constexpr uint16_t F_SHIFT = 15 - F_LSB;

   uint16_t sHex = (hex & S_MASK) >> S_SHIFT;
   uint16_t eHex = (hex & E_MASK) >> E_SHIFT;
   uint16_t fHex = (hex & F_MASK) >> F_SHIFT;

   float value;

   float s = std::pow(-1.0f, static_cast<float>(sHex));
   float e;
   float f;

   if (eHex == 0)
   {
      e = 2.0f;
      f = (fHex / std::pow(2.0f, 10.0f));
   }
   else
   {
      e = std::pow(2.0f, eHex - 16.0f);
      f = (1 + fHex / std::pow(2.0f, 10.0f));
   }

   value = s * e * f;

   return value;
}

float DecodeFloat32(uint16_t msw, uint16_t lsw)
{
   uint32_t value = msw << 16 | lsw;

   return reinterpret_cast<float&>(value);
}

} // namespace util
} // namespace scwx
