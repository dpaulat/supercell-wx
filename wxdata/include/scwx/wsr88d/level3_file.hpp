#pragma once

#include <scwx/wsr88d/nexrad_file.hpp>
#include <scwx/wsr88d/rpg/level3_message.hpp>

#include <memory>
#include <string>

namespace scwx
{
namespace wsr88d
{

class Level3FileImpl;

class Level3File : public NexradFile
{
public:
   explicit Level3File();
   ~Level3File();

   Level3File(const Level3File&) = delete;
   Level3File& operator=(const Level3File&) = delete;

   Level3File(Level3File&&) noexcept;
   Level3File& operator=(Level3File&&) noexcept;

   std::shared_ptr<rpg::Level3Message> message() const;

   bool LoadFile(const std::string& filename);
   bool LoadData(std::istream& is);

private:
   std::unique_ptr<Level3FileImpl> p;
};

} // namespace wsr88d
} // namespace scwx
