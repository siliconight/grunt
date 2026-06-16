#pragma once
// AudioOut.h — output-format dispatch. grunt defaults to OGG Vorbis; WAV is
// available for debugging and as the fallback when built without libvorbis.
#include "Types.h"
#include <string>

namespace voc {

enum class AudioFormat { Ogg, Wav };

// Returns the extension (without dot) for a format: "ogg" / "wav".
const char* format_ext(AudioFormat f);

// Parse a format name ("ogg"/"vorbis"/"wav"); defaults to Ogg on unknown.
AudioFormat format_from_string(const std::string& s);

// True if this build can actually encode OGG (libvorbis linked in).
bool ogg_supported();

// Write `buf` to `path` in `fmt`. If fmt==Ogg but this build lacks libvorbis,
// returns false with an explanatory error (caller may fall back to WAV).
bool write_audio(const std::string& path, const AudioBuffer& buf,
                 AudioFormat fmt, float quality, std::string& err);

} // namespace voc
