#pragma once
// Stages.h — pipeline stage interfaces (TDD §6).
#include "Types.h"
#include <string>
#include <vector>
#include <random>

namespace voc {

// §6.1
class TextNormalizer {
public:
    NormalizedText normalize(const std::string& text) const;
};

// §6.3 (grunt-mode planner: tokens -> playable syllable/grunt keys)
class SyllablePlanner {
public:
    UnitPlan plan(const NormalizedText& nt) const;
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

// §6.6 — stitch with zero-crossing align + crossfade, pitch/time, limiter
class AudioRenderer {
public:
    AudioBuffer render(const std::vector<SelectedUnit>& sel,
                       const UnitDatabase& db) const;
};

// §6.7 — PS1 FX presets
class RetroFxChain {
public:
    void apply(AudioBuffer& buf, const std::string& preset) const;
};

} // namespace voc
