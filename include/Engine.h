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

    // SPEECH PATH (the primary one for spoken lines): synthesize the WHOLE text
    // directly with Piper into one clean utterance, then style it — apply
    // character pitch/formant/sub/rasp to the whole buffer at its NATURAL length
    // (no unit stitching, no duration-fit), then the PS1 FX chain. This says any
    // words you type, intelligibly, then makes them sound retro. Needs a Piper
    // voice model present (model_id, e.g. "piper-en_US-ljspeech"); does NOT need
    // a loaded bank. The concatenative bank path (synth above) remains for
    // grunt-only banks; synth_vocalization remains for efforts/onomatopoeia.
    SynthResult synth_speech(const std::string& text,
                             const std::string& model_id,
                             const std::string& fx_preset,
                             const Options& opts,
                             double speed = 1.0,
                             const std::string& generator_override = "");

private:
    UnitDatabase db_;
    bool loaded_ = false;
};

} // namespace voc
