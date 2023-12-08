#include <scwx/qt/types/media_types.hpp>

#include <unordered_map>

namespace scwx
{
namespace qt
{
namespace types
{

static const std::unordered_map<AudioFile, std::string> audioFileInfo_ {
   {AudioFile::EasAttentionSignal,
    ":/res/audio/wikimedia/"
    "Emergency_Alert_System_Attention_Signal_20s.ogg"}};

const std::string& GetMediaPath(AudioFile audioFile)
{
   return audioFileInfo_.at(audioFile);
}

} // namespace types
} // namespace qt
} // namespace scwx
