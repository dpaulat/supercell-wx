#pragma once

#include <cstdint>
#include <istream>
#include <vector>

#include <openssl/evp.h>

namespace scwx
{
namespace util
{

bool ComputeDigest(const EVP_MD*              mdtype,
                   std::istream&              is,
                   std::vector<std::uint8_t>& digest);

} // namespace util
} // namespace scwx
