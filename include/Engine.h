#pragma once
// Engine.h — the single synthesis path shared by the CLI and the GUI.
// Both front ends call this; there is no second pipeline to drift.
#include "Stages.h"
#include "Types.h"
#include <string>
#include <cstdint>

namespace voc {

struct SynthResult {
    AudioBuffer audio;
    int units = 0;
    double peak_dbfs = 0.0;
    bool ok = false;
    std::string error;
};

// Owns a loaded voice bank; renders lines through the full pipeline.
class Engine {
public:
    // Load (or reload) a voice bank from a directory. Returns false on error.
    bool load_voice(const std::string& voice_dir, std::string& err);
    bool voice_loaded() const { return loaded_; }
    std::string voice_id() const { return loaded_ ? db_.voice_id() : std::string(); }
    int sample_rate() const { return loaded_ ? db_.sample_rate() : 22050; }

    const UnitDatabase& db() const { return db_; }

    // Render one line. Deterministic for a fixed seed.
    SynthResult synth(const std::string& text,
                      Emotion emotion,
                      const std::string& fx_preset,
                      uint64_t seed);

private:
    UnitDatabase db_;
    bool loaded_ = false;
};

} // namespace voc
