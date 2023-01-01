#pragma once

#include <streambuf>

namespace scwx
{
namespace qt
{
namespace util
{

/**
 * The QFileBuffer class is a std::streambuf interface to a QFile, allowing use
 * with C++ stream-based I/O.
 *
 * QFileBuffer has read-only support. Locales are ignored, and no conversions
 * are performed.
 *
 * Documentation for functions derived from
 * https://en.cppreference.com/ SPDX-License-Identifier: CC BY-SA 3.0
 */
class QFileBuffer : public std::streambuf
{
public:
   /**
    * Constructs a new QFileBuffer object. The created object is not associated
    * with a file, and is_open() returns false.
    */
   explicit QFileBuffer();

   /**
    * Constructs a new QFileBuffer object, then associated the object with a
    * file by calling open(filename, mode). If the open call is successful,
    * is_open() returns true.
    */
   explicit QFileBuffer(const std::string&      filename,
                        std::ios_base::openmode mode = std::ios_base::in);
   ~QFileBuffer();

   QFileBuffer(const QFileBuffer&)            = delete;
   QFileBuffer& operator=(const QFileBuffer&) = delete;

   QFileBuffer(QFileBuffer&&) noexcept;
   QFileBuffer& operator=(QFileBuffer&&) noexcept;

   /**
    * @brief Checks if the associated file is open
    *
    * Returns true if the most recent call to open() succeeded and there has
    * been no call to close() since then.
    *
    * @return true if the associated file is open, false otherwise
    */
   bool is_open() const;

   /**
    * @brief Opens a file and configures it as the associated character sequence
    *
    * Opens the file with the given name. If the associated file was already
    * open, returns a null pointer right away.
    *
    * @param filename The file name to open
    * @param openmode The file opening mode, a binary OR of the std::ios_base
    * modes
    *
    * @return this on success, a null pointer on failure
    */
   QFileBuffer* open(const std::string&      filename,
                     std::ios_base::openmode mode = std::ios_base::in);

   /**
    * @brief Flushes the put area buffer and closes the associated file
    *
    * Closes the file, regardless of whether any of the preceding calls
    * succeeded or failed.
    *
    * If any of the function calls made fails, returns a null pointer. If any of
    * the function calls made throws an exception, the exception is caught and
    * rethrown after closing. If the file is already closed, returns a null
    * pointer right away.
    *
    * @return this on success, a null pointer on failure
    */
   QFileBuffer* close();

protected:
   /**
    * @brief Backs out the input sequence to unget a character, not affecting
    * the associated file
    *
    * @param c The character to put back, or Traits::eof() to indicate that
    * backing up of the get area is requested
    *
    * @return c on success except if c was Traits::eof(), in which case
    * Traits::not_eof(c) is returned. Traits::eof() on failure.
    */
   virtual int_type pbackfail(int_type c = traits_type::eof()) override;

   /**
    * @brief Repositions the file position, using relative addressing
    *
    * Sets the position indicator of the input and/or output sequence relative
    * to some other position.
    *
    * @param off Relative position to set the position indicator to
    * @param dir Defines base position to apply the relative offset to
    * @param which Defines which of the input and/or output sequences to affect
    *
    * @return The resulting absolute position as defined by the position
    * indicator.
    */
   virtual pos_type
   seekoff(off_type                off,
           std::ios_base::seekdir  dir,
           std::ios_base::openmode which = std::ios_base::in |
                                           std::ios_base::out) override;

   /**
    * @brief Repositions the file position, using absolute addressing
    *
    * Sets the position indicator of the input and/or output sequence to an
    * absolute position.
    *
    * @param pos Absolute position to set the position indicator to
    * @param which Defines which of the input and/or output sequences to affect
    *
    * @return The resulting absolute position as defined by the position
    * indicator.
    */
   virtual pos_type
   seekpos(pos_type                pos,
           std::ios_base::openmode which = std::ios_base::in |
                                           std::ios_base::out) override;

   /**
    * @brief Reads from the associated file
    *
    * Ensures that at least one character is available in the input area by
    * updating the pointers to the input area (if needed) and reading more data
    * in from the input sequence (if applicable). Returns the value of that
    * character (converted to int_type with Traits::to_int_type(c)) on success
    * or Traits::eof() on failure.
    *
    * The function may update gptr, egptr and eback pointers to define the
    * location of newly loaded data (if any). On failure, the function ensures
    * that either gptr() == nullptr or gptr() == egptr.
    *
    * @return The value of the character pointed to by the get pointer after the
    * call on success, or Traits::eof() otherwise.
    */
   virtual int_type underflow() override;

   /**
    * @brief Reads from the associated file and advances the next pointer in the
    * get area
    *
    * Behaves like the underflow(), except that if underflow() succeeds (does
    * not return Traits::eof()), then advances the next pointer for the get
    * area. In orther words, consumes one of the characters obtained by
    * underflow().
    *
    * @return The value of the character that was read and consumed in case of
    * success, or Traits::eof() in case of failure.
    */
   virtual int_type uflow() override;

   /**
    * @brief Reads multiple characters from the input sequence
    *
    * Reads count characters from the input sequence and stores them into a
    * character array pointed to by s. The characters are read as if by repeated
    * calls to sbumpc(). That is, if less than count characters are immediately
    * available, the function calls uflow() to provide more until Traits::eof()
    * is returned.
    *
    * @param s Pointer to the beginning of a char_type array
    * @param count Maximum number of characters to read
    *
    * @return The number of characters successfully read. If it is less than
    * count the input sequence has reached the end.
    */
   virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override;

private:
   class Impl;
   std::unique_ptr<Impl> p;
};

} // namespace util
} // namespace qt
} // namespace scwx
