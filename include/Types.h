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

// ---- Stage 2 output: PhonemeSeq (TDD §6.2, Phase 1) ------------------------
// A word resolved to ARPAbet phonemes, plus how it was resolved (for the
// unknown-word coverage report).
enum class PhonemeSource { Dictionary, RuleFallback, Passthrough };

struct WordPhonemes {
    std::string word;                     // the source token
    std::vector<std::string> phonemes;    // ARPAbet, e.g. {"G","EY","T"}
    PhonemeSource source = PhonemeSource::Dictionary;
    bool is_emphasis = false;
};

struct PhonemeSeq {
    std::vector<WordPhonemes> words;
    Emotion emotion = Emotion::Neutral;
    std::string terminal_punct;
};

// Unit kinds in a voice bank (declared early — RequestedUnit references it).
enum class UnitType { Phoneme, Diphone, Syllable, Word, Grunt, Effort };

// ---- Stage 2/3 output: UnitPlan (TDD §6.2/6.3) -----------------------------
// A requested unit carries a primary key plus an ordered fallback chain:
// syllable -> its phonemes -> grunt. The selector walks the chain until the
// bank has a matching unit. `fallback` lists alternative keys in priority order
// (the primary `key` is tried first, then these).
struct RequestedUnit {
    std::string key;                   // primary: syllable key (e.g. "G EY T" joined) or word
    UnitType    preferred = UnitType::Syllable;
    std::vector<std::string> fallback; // ordered: phoneme keys, then "" = any grunt
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
    std::vector<std::string> fallback;   // ordered alternative keys ("" = grunt)
    int    duration_ms      = 120;
    double pitch_offset_st  = 0.0;     // semitones
    double gain_db          = 0.0;
    bool   stress           = false;
    // Phase 3 character DSP (0/false = no effect)
    double formant_shift    = 0.0;     // -1..+1; <0 = bigger/darker, >0 = smaller/brighter
    bool   sub_layer        = false;   // add a sub-octave layer for chest/size
    double rasp             = 0.0;     // 0..1; adds gritty saturation
};

struct ProsodyPlan {
    std::vector<ProsodyUnit> units;
    Emotion emotion = Emotion::Neutral;
};

// ---- Voice bank: AudioUnit metadata (TDD §9) -------------------------------
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
