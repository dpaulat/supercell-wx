#include <scwx/types/ntp_types.hpp>

#include <cassert>
#include <string>

#ifdef _WIN32
#   include <WinSock2.h>
#else
#   include <arpa/inet.h>
#endif

namespace scwx::types::ntp
{

NtpPacket NtpPacket::Parse(const std::span<std::uint8_t> data)
{
   NtpPacket packet {};

   assert(data.size() >= sizeof(NtpPacket));

   packet = *reinterpret_cast<const NtpPacket*>(data.data());

   packet.rootDelay      = ntohl(packet.rootDelay);
   packet.rootDispersion = ntohl(packet.rootDispersion);
   packet.refId          = ntohl(packet.refId);

   packet.refTm_s = ntohl(packet.refTm_s);
   packet.refTm_f = ntohl(packet.refTm_f);

   packet.origTm_s = ntohl(packet.origTm_s);
   packet.origTm_f = ntohl(packet.origTm_f);

   packet.rxTm_s = ntohl(packet.rxTm_s);
   packet.rxTm_f = ntohl(packet.rxTm_f);

   packet.txTm_s = ntohl(packet.txTm_s);
   packet.txTm_f = ntohl(packet.txTm_f);

   return packet;
}

} // namespace scwx::types::ntp
