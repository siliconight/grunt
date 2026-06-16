#pragma once
// Types.h — the typed data contracts passed between pipeline stages (TDD §13).
// One stage takes a struct and returns a struct; no stage reaches back.

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace voc {

// Portable pi — MSVC doesn't define M_PI without _USE_MATH_DEFINES, so we
// avoid the platform dependency entirely.
inline constexpr double kPi = 3.14159265358979323846;

// ---- Emotion ---------------------------------------------------------------
enum class Emotion { Neutral, Urgent, Angry };

Emotion emotion_from_string(const std::string& s);
const char* emotion_to_string(Emotion e);

// ---- Stage 1 output: NormalizedText (TDD §6.1) -----------------------------
struct NormalizedText {
    std::vector<std::string> tokens;   // lowercased words
    std::string terminal_punct;        // "!", "?", ".", "" ...
    Emotion emotion_hint = Emotion::Neutral;
    std::vector<std::string> emphasis_words;
};

// ---- Stage 2/3 output: UnitPlan (TDD §6.2/6.3) -----------------------------
// In Phase 0 (grunt mode) a requested unit is just a syllable-ish token plus a
// fallback chain. Later phases enrich this with phonemes/diphones.
struct RequestedUnit {
    std::string key;                   // e.g. "geh", "tah", or a word like "no"
    bool is_emphasis = false;
};

struct UnitPlan {
    std::vector<RequestedUnit> units;
    Emotion emotion = Emotion::Neutral;
    std::string terminal_punct;
};

// ---- Stage 4 output: ProsodyPlan (TDD §6.4) --------------------------------
struct ProsodyUnit {
    std::string key;
    int    duration_ms      = 120;
    double pitch_offset_st  = 0.0;     // semitones
    double gain_db          = 0.0;
    bool   stress           = false;
};

struct ProsodyPlan {
    std::vector<ProsodyUnit> units;
    Emotion emotion = Emotion::Neutral;
};

// ---- Voice bank: AudioUnit metadata (TDD §9) -------------------------------
enum class UnitType { Phoneme, Diphone, Syllable, Word, Grunt, Effort };

struct Provenance {
    std::string source = "original_recording"; // original_recording|contracted_vo|sample_pack|synth_placeholder
    std::string recorded_by;
    std::string license = "owned";
    bool commercial_use = true;
    bool synth_tool_derived = false;
};

struct AudioUnit {
    std::string id;
    UnitType    type = UnitType::Grunt;
    std::string key;                   // matching key (syllable text / word / grunt tag)
    Emotion     emotion = Emotion::Neutral;
    double      pitch_center_hz = 0.0;
    int         duration_ms = 0;
    double      energy = 0.0;
    std::string file;                  // relative to voice dir
    Provenance  provenance;
};

// ---- Selected units (TDD §6.5) ---------------------------------------------
struct SelectedUnit {
    const AudioUnit* unit = nullptr;
    ProsodyUnit prosody;
};

// ---- Audio buffer ----------------------------------------------------------
struct AudioBuffer {
    int sample_rate = 22050;
    std::vector<float> samples;        // mono, [-1, 1]
    double peak_dbfs() const;
};

} // namespace voc
