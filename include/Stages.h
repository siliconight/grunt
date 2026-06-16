#pragma once
// Stages.h — pipeline stage interfaces (TDD §6).
#include "Types.h"
#include <string>
#include <vector>
#include <random>
#include <unordered_map>

namespace voc {

// §6.1
class TextNormalizer {
public:
    NormalizedText normalize(const std::string& text) const;
};

// §6.2 (Phase 1) — words -> ARPAbet phonemes via dictionary + rule fallback
class PhonemeMapper {
public:
    // Load a CMUdict-style dictionary file (WORD  P1 P2 ...). Optional —
    // without it, every word uses the rule-based G2P fallback.
    bool load_dictionary(const std::string& path, std::string& err);
    size_t dictionary_size() const { return dict_.size(); }

    PhonemeSeq map(const NormalizedText& nt) const;

    // expose single-word resolution for the debug command / tests
    WordPhonemes map_word(const std::string& word) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> dict_;
    // rule-based grapheme->phoneme fallback for out-of-dictionary words
    std::vector<std::string> g2p_fallback(const std::string& word) const;
};

// §6.3 (grunt-mode planner: tokens -> playable syllable/grunt keys)
class SyllablePlanner {
public:
    // Phase 0 grunt-mode: spelling-based splitter (kept as fallback when no
    // phoneme mapper is available).
    UnitPlan plan(const NormalizedText& nt) const;

    // Phase 2: phoneme-backed planning. Syllabifies each word from its ARPAbet
    // phonemes and builds a scored fallback chain per unit:
    // syllable key -> constituent phoneme keys -> grunt. Supersedes the Phase 0
    // splitter when a mapper is provided.
    UnitPlan plan_phonemic(const NormalizedText& nt, const PhonemeMapper& mapper) const;

private:
    // crude open-syllable splitter for grunt mode; good enough to drive rhythm
    std::vector<std::string> syllabify(const std::string& word) const;
};

// §6.4
class ProsodyPlanner {
public:
    ProsodyPlan plan(const UnitPlan& up) const;
};

// Voice bank (§7-9) — loads voice.json + metadata/units.json
class UnitDatabase {
public:
    bool load(const std::string& voice_dir, std::string& err);

    std::string voice_id() const { return voice_id_; }
    int sample_rate() const { return sample_rate_; }
    double base_pitch() const { return base_pitch_; }
    const std::string& dir() const { return dir_; }

    // candidates whose key matches, plus all grunts as universal fallback
    std::vector<const AudioUnit*> candidates(const std::string& key,
                                             Emotion emotion) const;
    // exact key matches only (no grunt auto-append); used to walk a scored
    // fallback chain explicitly. Empty key returns all grunts.
    std::vector<const AudioUnit*> match_key(const std::string& key) const;
    const std::vector<AudioUnit>& all() const { return units_; }

private:
    std::string dir_;
    std::string voice_id_;
    int sample_rate_ = 22050;
    double base_pitch_ = 0.0;
    std::vector<AudioUnit> units_;
};

// §6.5 — Phase 0: target+repetition cost, greedy (Viterbi arrives in Phase 3)
class UnitSelector {
public:
    explicit UnitSelector(uint64_t seed) : rng_(seed) {}
    std::vector<SelectedUnit> select(const ProsodyPlan& pp,
                                     const UnitDatabase& db);
private:
    std::mt19937_64 rng_;
};

// Selection cost functions — exposed for testing. (Defined in UnitSelector.cpp.)
namespace sel {
double join_cost(const AudioUnit* a, const AudioUnit* b);
}

// §6.6 — stitch with zero-crossing align + crossfade, pitch/time, limiter
class AudioRenderer {
public:
    AudioBuffer render(const std::vector<SelectedUnit>& sel,
                       const UnitDatabase& db) const;
};

// Phase 3 character DSP — exposed for testing. (Defined in AudioRenderer.cpp.)
namespace dsp {
std::vector<float> formant_shift(const std::vector<float>& in, double shift);
std::vector<float> add_sub_octave(const std::vector<float>& in, double mix);
void apply_rasp(std::vector<float>& s, double amt);
size_t estimate_period(const std::vector<float>& s, int sr);
bool psola_timestretch(const std::vector<float>& in, int sr,
                       double time_ratio, std::vector<float>& out);
bool psola(const std::vector<float>& in, int sr,
           double pitch_ratio, double time_ratio, std::vector<float>& out);
}

// §6.7 — PS1 FX presets
class RetroFxChain {
public:
    void apply(AudioBuffer& buf, const std::string& preset) const;
};

} // namespace voc
