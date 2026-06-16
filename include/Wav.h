#pragma once
// Wav.h — minimal mono 16-bit PCM WAV read/write (TDD §13 WavWriter).
#include "Types.h"
#include <string>

namespace voc {
bool write_wav(const std::string& path, const AudioBuffer& buf, std::string& err);
bool read_wav(const std::string& path, AudioBuffer& out, std::string& err);
}
