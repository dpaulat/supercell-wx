#pragma once

#include <scwx/util/iterator.hpp>

#include <string>

namespace scwx
{
namespace qt
{
namespace types
{

enum class AudioFile
{
   EasAttentionSignal
};
typedef scwx::util::Iterator<AudioFile,
                             AudioFile::EasAttentionSignal,
                             AudioFile::EasAttentionSignal>
   AudioFileIterator;

const std::string& GetMediaPath(AudioFile audioFile);

} // namespace types
} // namespace qt
} // namespace scwx
