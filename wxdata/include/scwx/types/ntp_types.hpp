#pragma once

#include <cstdint>
#include <span>

namespace scwx::types::ntp
{

/* Adapted from:
 * https://github.com/lettier/ntpclient/blob/master/source/c/main.c
 *
 * Copyright (c) 2014 David Lettier
 * Copyright (c) 2020 Krystian Stasiowski
 * Distributed under the BSD 3-Clause License (See
 * https://github.com/lettier/ntpclient/blob/master/LICENSE)
 */

#pragma pack(push, 1)

struct NtpPacket
{
   struct LiVnMode
   {
      std::uint8_t mode : 3; // Client will pick mode 3 for client.
      std::uint8_t vn : 3;   // Version number of the protocol.
      std::uint8_t li : 2;   // Leap indicator.
   };

   union
   {
      std::uint8_t li_vn_mode;
      LiVnMode     fields;
   };

   std::uint8_t stratum;   // Stratum level of the local clock.
   std::uint8_t poll;      // Maximum interval between successive messages.
   std::uint8_t precision; // Precision of the local clock.

   std::uint32_t rootDelay;      // Total round trip delay time.
   std::uint32_t rootDispersion; // Max error aloud from primary clock source.
   std::uint32_t refId;          // Reference clock identifier.

   std::uint32_t refTm_s; // Reference time-stamp seconds.
   std::uint32_t refTm_f; // Reference time-stamp fraction of a second.

   std::uint32_t origTm_s; // Originate time-stamp seconds.
   std::uint32_t origTm_f; // Originate time-stamp fraction of a second.

   std::uint32_t rxTm_s; // Received time-stamp seconds.
   std::uint32_t rxTm_f; // Received time-stamp fraction of a second.

   std::uint32_t txTm_s; // Transmit time-stamp seconds.
   std::uint32_t txTm_f; // Transmit time-stamp fraction of a second.

   static NtpPacket Parse(const std::span<std::uint8_t> data);
};
// Total: 48 bytes.

#pragma pack(pop)

} // namespace scwx::types::ntp
