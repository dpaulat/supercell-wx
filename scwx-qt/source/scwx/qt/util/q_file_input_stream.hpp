#pragma once

#include <istream>
#include <memory>

namespace scwx
{
namespace qt
{
namespace util
{

class QFileBuffer;

/**
 * The QFileInputStream class is a std::istream interface to a QFile, allowing
 * use with C++ stream-based I/O.
 *
 * Documentation for functions derived from
 * https://en.cppreference.com/ SPDX-License-Identifier: CC BY-SA 3.0
 */
class QFileInputStream : public std::istream
{
public:
   /**
    * Constructs a new QFileInputStream object. The created object is not
    * associated with a file: default-constructs the QFileBuffer and constructs
    * the base with the pointer to this default-constructed QFileBuffer member.
    */
   explicit QFileInputStream();

   /**
    * Constructs a new QFileInputStream object, then associates the object with
    * a file by calling rdbuf()->open(filename, mode | std::ios_base::in). If
    * the open() call returns a null pointer, sets setstate(failbit).
    */
   explicit QFileInputStream(const std::string&      filename,
                             std::ios_base::openmode mode = std::ios_base::in);
   ~QFileInputStream();

   QFileInputStream(const QFileInputStream&)            = delete;
   QFileInputStream& operator=(const QFileInputStream&) = delete;

   QFileInputStream(QFileInputStream&&) noexcept;
   QFileInputStream& operator=(QFileInputStream&&) noexcept;

   /**
    * @brief Swaps two QFileInputStream objects
    *
    * Exchanges the contents of the QFile input stream with those of the other.
    */
   void swap(QFileInputStream& other);

   /**
    * @brief Checks if the stream has an associated file
    *
    * Checks if the file stream has an associated file. Effectively calls
    * rdbuf()->is_open().
    *
    * @return true if the associated file is open, false otherwise
    */
   bool is_open() const;

   /**
    * @brief Opens a file and associates it with the stream
    *
    * Opens and associates the file with name filename with the file stream.
    * Effectively calls rdbuf()->open(filename, mode | ios_base::in). Calls
    * setstate(failbit) on failure.
    *
    * @param filename The file name to open
    * @param openmode The file opening mode, a binary OR of the std::ios_base
    * modes
    */
   void open(const std::string&      filename,
             std::ios_base::openmode mode = std::ios_base::in);

   /**
    * @brief Close the associated file
    *
    * Closes the associated file. Effectively calls rdbuf()->close(). If an
    * error occurs during operation, setstate(failbit) is called.
    */
   void close();

   /**
    * @brief Returns the underlying raw file device object
    *
    * Returns a pointer to the underlying raw file device object.
    *
    * @return Pointer to the underlying raw file device
    */
   QFileBuffer* rdbuf() const;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
