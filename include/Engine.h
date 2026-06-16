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

    // Optional per-render character layering (from a CharacterPreset).
    struct Options {
        double extra_pitch_st = 0.0;  // added to every unit's pitch offset
        double extra_gain_db  = 0.0;  // added to every unit's gain
        double formant_shift  = 0.0;  // character formant shift (-1..+1)
        bool   sub_layer      = false;// add sub-octave layer
        double rasp           = 0.0;  // 0..1 grit
    };

    // Render one line. Deterministic for a fixed seed.
    SynthResult synth(const std::string& text,
                      Emotion emotion,
                      const std::string& fx_preset,
                      uint64_t seed);

    // With character layering (pitch/gain offsets).
    SynthResult synth(const std::string& text,
                      Emotion emotion,
                      const std::string& fx_preset,
                      uint64_t seed,
                      const Options& opts);

    // Render a non-lexical vocalization (effort / onomatopoeia) from a phoneme
    // sequence, bypassing text normalization + syllable planning. intensity
    // (0..1) scales gain and duration. Composes with character Options.
    SynthResult synth_vocalization(const PhonemeSeq& seq,
                                   double intensity,
                                   const std::string& fx_preset,
                                   uint64_t seed,
                                   const Options& opts);

private:
    UnitDatabase db_;
    bool loaded_ = false;
};

} // namespace voc
