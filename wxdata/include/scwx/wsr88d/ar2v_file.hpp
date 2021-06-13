#pragma once

#include <memory>
#include <string>

namespace scwx
{
namespace wsr88d
{

class Ar2vFileImpl;

/**
 * @brief The Archive II file is specified in the Interface Control Document for
 * the Archive II/User, Document Number 2620010H, published by the WSR-88D Radar
 * Operations Center.
 */
class Ar2vFile
{
public:
   explicit Ar2vFile();
   ~Ar2vFile();

   Ar2vFile(const Ar2vFile&) = delete;
   Ar2vFile& operator=(const Ar2vFile&) = delete;

   Ar2vFile(Ar2vFile&&) noexcept;
   Ar2vFile& operator=(Ar2vFile&&);

   bool LoadFile(const std::string& filename);

private:
   std::unique_ptr<Ar2vFileImpl> p;
};

} // namespace wsr88d
} // namespace scwx
