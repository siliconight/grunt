#include "Stages.h"
#include "Wav.h"
#include <cmath>
#include <algorithm>

namespace voc {

namespace {

// resample (linear) to change pitch+speed together, then we correct duration
std::vector<float> resample(const std::vector<float>& in, double ratio) {
    if (in.empty() || ratio <= 0.0) return {};
    size_t out_n = (size_t)std::max<double>(1.0, in.size() / ratio);
    std::vector<float> out(out_n);
    for (size_t i = 0; i < out_n; ++i) {
        double src = i * ratio;
        size_t i0 = (size_t)src;
        double frac = src - i0;
        float a = in[std::min(i0, in.size() - 1)];
        float b = in[std::min(i0 + 1, in.size() - 1)];
        out[i] = (float)(a + (b - a) * frac);
    }
    return out;
}

// crude time-stretch to a target sample count by linear index mapping.
// (PSOLA/WSOLA arrives in a later phase; PS1 FX masks the artifacts.)
std::vector<float> fit_to_length(const std::vector<float>& in, size_t target_n) {
    if (in.empty() || target_n == 0) return std::vector<float>(target_n, 0.f);
    if (in.size() == target_n) return in;
    std::vector<float> out(target_n);
    double ratio = (double)in.size() / (double)target_n;
    for (size_t i = 0; i < target_n; ++i) {
        double src = i * ratio;
        size_t i0 = (size_t)src;
        double frac = src - i0;
        float a = in[std::min(i0, in.size() - 1)];
        float b = in[std::min(i0 + 1, in.size() - 1)];
        out[i] = (float)(a + (b - a) * frac);
    }
    return out;
}

// nudge an index to the nearest zero crossing within a small window
size_t nearest_zero_crossing(const std::vector<float>& s, size_t idx, size_t window) {
    if (s.empty()) return 0;
    idx = std::min(idx, s.size() - 1);
    for (size_t d = 0; d < window; ++d) {
        if (idx + d + 1 < s.size() && ((s[idx + d] <= 0) != (s[idx + d + 1] <= 0))) return idx + d;
        if (idx >= d + 1 && ((s[idx - d] <= 0) != (s[idx - d - 1] <= 0))) return idx - d;
    }
    return idx;
}

void apply_gain_db(std::vector<float>& s, double db) {
    float g = (float)std::pow(10.0, db / 20.0);
    for (float& x : s) x *= g;
}

} // namespace

AudioBuffer AudioRenderer::render(const std::vector<SelectedUnit>& sel,
                                  const UnitDatabase& db) const {
    AudioBuffer out;
    out.sample_rate = db.sample_rate();
    const int sr = out.sample_rate;
    const size_t xfade = (size_t)(sr * 0.006); // 6 ms crossfade

    for (const auto& su : sel) {
        if (!su.unit) continue;

        AudioBuffer clip;
        std::string err;
        if (!read_wav(db.dir() + "/" + su.unit->file, clip, err)) continue;
        if (clip.samples.empty()) continue;

        std::vector<float> s = clip.samples;

        // pitch shift via resample ratio (positive semitones -> higher pitch)
        double ratio = std::pow(2.0, su.prosody.pitch_offset_st / 12.0);
        if (std::fabs(su.prosody.pitch_offset_st) > 1e-3) s = resample(s, ratio);

        // fit to target duration
        size_t target_n = (size_t)((double)su.prosody.duration_ms * sr / 1000.0);
        if (target_n > 0) s = fit_to_length(s, target_n);

        // per-unit gain
        apply_gain_db(s, su.prosody.gain_db);

        // short fade in/out on the unit to avoid edge clicks
        size_t edge = std::min<size_t>(s.size() / 4, (size_t)(sr * 0.003));
        for (size_t i = 0; i < edge; ++i) {
            float g = (float)i / (float)edge;
            s[i] *= g;
            s[s.size() - 1 - i] *= g;
        }

        if (out.samples.empty()) {
            out.samples = std::move(s);
            continue;
        }

        // crossfade-join at a zero crossing near the tail
        size_t join = out.samples.size() > xfade ? out.samples.size() - xfade : 0;
        join = nearest_zero_crossing(out.samples, join, xfade);
        size_t overlap = std::min(out.samples.size() - join, std::min(xfade, s.size()));
        if (overlap > s.size()) overlap = s.size(); // explicit bound for the compiler

        for (size_t i = 0; i < overlap; ++i) {
            float t = (float)i / (float)overlap;
            out.samples[join + i] = out.samples[join + i] * (1.f - t) + s[i] * t;
        }
        for (size_t i = overlap; i < s.size(); ++i)
            out.samples.push_back(s[i]);
    }

    // brick-wall-ish limiter to keep peaks under -1 dBFS (TDD acceptance)
    const float ceiling = 0.891f; // ~ -1 dBFS
    float peak = 0.f;
    for (float x : out.samples) peak = std::max(peak, std::fabs(x));
    if (peak > ceiling) {
        float g = ceiling / peak;
        for (float& x : out.samples) x *= g;
    }

    return out;
}

} // namespace voc
