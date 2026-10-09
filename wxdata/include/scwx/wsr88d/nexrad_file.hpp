#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace scwx
{
namespace wsr88d
{

class NexradFileImpl;

class NexradFile
{
public:
   NexradFile(const NexradFile&)            = delete;
   NexradFile& operator=(const NexradFile&) = delete;

   virtual ~NexradFile();

   virtual bool LoadFile(const std::string& filename) = 0;
   virtual bool LoadData(std::istream& is)            = 0;

   /**
    * @brief Original file bytes captured when the product was loaded.
    *
    * Gzip-compressed products retain the compressed payload so other
    * applications receive the same data that was downloaded.
    */
   [[nodiscard]] const std::vector<std::uint8_t>& file_data() const;

   /**
    * @brief Source filename (basename) associated with the original payload.
    */
   [[nodiscard]] const std::string& filename() const;

   [[nodiscard]] bool has_file_data() const;
   [[nodiscard]] bool is_gzip_compressed() const;

   void set_file_data(std::vector<std::uint8_t> data);
   void set_filename(std::string filename);
   void set_filename_from_path(const std::string& path);

   /**
    * @brief Writes the original file bytes to disk.
    *
    * @param [in] filename Destination path
    *
    * @return Whether the file was written successfully
    */
   [[nodiscard]] bool SaveFile(const std::string& filename) const;

protected:
   explicit NexradFile();

   NexradFile(NexradFile&&) noexcept;
   NexradFile& operator=(NexradFile&&) noexcept;

private:
   std::unique_ptr<NexradFileImpl> p;
};

} // namespace wsr88d
} // namespace scwx
