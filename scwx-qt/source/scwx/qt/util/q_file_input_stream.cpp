#include <scwx/qt/util/q_file_input_stream.hpp>
#include <scwx/qt/util/q_file_buffer.hpp>

namespace scwx
{
namespace qt
{
namespace util
{

static const std::string logPrefix_ = "scwx::qt::util::q_file_input_stream";

// Adapted from Microsoft ifstream reference implementation
// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

class QFileInputStream::Impl
{
public:
   explicit Impl() : buffer_ {} {};
   explicit Impl(const std::string& filename, std::ios_base::openmode mode) :
       buffer_ {filename, mode} {};
   ~Impl() = default;

   QFileBuffer buffer_;
};

QFileInputStream::QFileInputStream() :
    std::istream(nullptr), p(std::make_unique<Impl>())
{
   std::basic_ios<char_type, traits_type>::rdbuf(&p->buffer_);
}

QFileInputStream::QFileInputStream(const std::string&      filename,
                                   std::ios_base::openmode mode) :
    std::istream(nullptr),
    p(std::make_unique<Impl>(filename, mode | std::ios_base::in))
{
   std::basic_ios<char_type, traits_type>::rdbuf(&p->buffer_);
   if (!p->buffer_.is_open())
   {
      setstate(std::ios_base::failbit);
   }
}

QFileInputStream::~QFileInputStream() = default;

QFileInputStream::QFileInputStream(QFileInputStream&& other) noexcept :
    std::istream(nullptr), p(std::make_unique<Impl>())
{
   swap(other);
};
QFileInputStream&
QFileInputStream::operator=(QFileInputStream&&) noexcept = default;

void QFileInputStream::swap(QFileInputStream& other)
{
   // Swap the base class and managed implementation pointer
   std::istream::swap(other);
   p.swap(other.p);
}

bool QFileInputStream::is_open() const
{
   return p->buffer_.is_open();
}

void QFileInputStream::open(const std::string&      filename,
                            std::ios_base::openmode mode)
{
   if (p->buffer_.open(filename, mode | std::ios_base::in) == nullptr)
   {
      setstate(std::ios_base::failbit);
   }
   else
   {
      clear();
   }
}

void QFileInputStream::close()
{
   if (p->buffer_.close() == nullptr)
   {
      setstate(std::ios_base::failbit);
   }
}

QFileBuffer* QFileInputStream::rdbuf() const
{
   return &p->buffer_;
}

} // namespace util
} // namespace qt
} // namespace scwx
