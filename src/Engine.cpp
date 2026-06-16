#include "Engine.h"
#include <cctype>

namespace voc {

bool Engine::load_voice(const std::string& voice_dir, std::string& err) {
    UnitDatabase fresh;
    if (!fresh.load(voice_dir, err)) { loaded_ = false; return false; }
    db_ = std::move(fresh);
    loaded_ = true;
    return true;
}

SynthResult Engine::synth(const std::string& text, Emotion emotion,
                          const std::string& fx_preset, uint64_t seed) {
    return synth(text, emotion, fx_preset, seed, Options{});
}

SynthResult Engine::synth(const std::string& text,
                          Emotion emotion,
                          const std::string& fx_preset,
                          uint64_t seed,
                          const Options& opts) {
    SynthResult r;
    if (!loaded_) { r.error = "no voice bank loaded"; return r; }

    TextNormalizer norm;
    SyllablePlanner syl;
    ProsodyPlanner pros;
    UnitSelector sel(seed);
    AudioRenderer rend;
    RetroFxChain fx;

    NormalizedText nt = norm.normalize(text);
    UnitPlan up = syl.plan(nt);
    if (emotion != Emotion::Neutral) up.emotion = emotion; // explicit override
    ProsodyPlan pp = pros.plan(up);

    // layer character pitch/gain over every unit
    if (opts.extra_pitch_st != 0.0 || opts.extra_gain_db != 0.0) {
        for (auto& u : pp.units) {
            u.pitch_offset_st += opts.extra_pitch_st;
            u.gain_db += opts.extra_gain_db;
        }
    }

    auto selected = sel.select(pp, db_);

    r.audio = rend.render(selected, db_);
    fx.apply(r.audio, fx_preset);
    r.units = (int)selected.size();
    r.peak_dbfs = r.audio.peak_dbfs();
    r.ok = true;
    return r;
}

SynthResult Engine::synth_vocalization(const PhonemeSeq& seq,
                                       double intensity,
                                       const std::string& fx_preset,
                                       uint64_t seed,
                                       const Options& opts) {
    SynthResult r;
    if (!loaded_) { r.error = "no voice bank loaded"; return r; }

    ProsodyPlanner pros;
    UnitSelector sel(seed);
    AudioRenderer rend;
    RetroFxChain fx;

    // Build a UnitPlan directly from the phoneme sequence: each phoneme becomes
    // a requested unit key (lowercased ARPAbet). The selector matches bank
    // units by key and falls back to grunts, so this works whether or not the
    // bank has dedicated effort units.
    UnitPlan up;
    up.emotion = seq.emotion;
    up.terminal_punct = seq.terminal_punct;
    for (const auto& w : seq.words) {
        for (const auto& ph : w.phonemes) {
            RequestedUnit ru;
            ru.key = ph;
            for (auto& ch : ru.key) ch = (char)std::tolower((unsigned char)ch);
            ru.is_emphasis = w.is_emphasis;
            up.units.push_back(ru);
        }
    }
    if (up.units.empty()) { r.error = "vocalization produced no units"; return r; }

    ProsodyPlan pp = pros.plan(up);

    // intensity (0..1) -> extra gain (up to +6 dB) and longer holds (up to +60%)
    double gain_boost = 6.0 * intensity;
    double dur_scale  = 1.0 + 0.6 * intensity;
    for (auto& u : pp.units) {
        u.gain_db += gain_boost + opts.extra_gain_db;
        u.pitch_offset_st += opts.extra_pitch_st;
        u.duration_ms = (int)(u.duration_ms * dur_scale);
    }

    auto selected = sel.select(pp, db_);
    r.audio = rend.render(selected, db_);
    fx.apply(r.audio, fx_preset);
    r.units = (int)selected.size();
    r.peak_dbfs = r.audio.peak_dbfs();
    r.ok = true;
    return r;
}

} // namespace voc
