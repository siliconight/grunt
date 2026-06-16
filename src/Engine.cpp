#include "Engine.h"

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

} // namespace voc
