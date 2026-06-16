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

namespace dsp {

// Formant shift via resample-and-restore: resampling by `fr` moves the spectral
// envelope (formants), then we restore the original length so PITCH is
// unchanged but the formants have moved. fr<1 lowers formants (bigger/darker
// throat — orc/demon); fr>1 raises them (smaller/brighter). This is the
// classic dependency-free formant trick; the later pitch resample is applied
// separately, so pitch and formants are decoupled.
std::vector<float> formant_shift(const std::vector<float>& in, double shift) {
    if (in.empty() || std::fabs(shift) < 1e-3) return in;
    // shift in [-1,1] -> ratio roughly [0.7, 1.43]
    double fr = std::pow(2.0, shift * 0.5);          // semitone-ish mapping
    size_t n = in.size();
    // resample to n/fr samples (moves formants), then stretch back to n
    std::vector<float> moved = resample(in, fr);     // length ~ n/fr
    return fit_to_length(moved, n);                   // restore length -> pitch intact
}

// Sub-octave layer: add a copy pitched down one octave (stretch x2 then refit)
// for chest/size. Mixed under the dry signal.
std::vector<float> add_sub_octave(const std::vector<float>& in, double mix) {
    if (in.empty()) return in;
    std::vector<float> sub = resample(in, 0.5);      // down an octave (longer)
    sub = fit_to_length(sub, in.size());
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
        out[i] = (float)(in[i] + mix * sub[i]);
    return out;
}

// Rasp: gritty waveshaping (soft-clip + a touch of odd harmonics) for raspy /
// monstrous timbres. amt in 0..1.
void apply_rasp(std::vector<float>& s, double amt) {
    if (amt < 1e-3) return;
    double drive = 1.0 + 6.0 * amt;
    for (float& x : s) {
        double v = std::tanh(x * drive);             // soft clip
        v += amt * 0.15 * std::sin(3.0 * x * kPi);   // odd-harmonic grit
        x = (float)std::max(-1.0, std::min(1.0, v));
    }
}

// ---- PSOLA (Pitch-Synchronous Overlap-Add) --------------------------------
// Repitch and/or retime voiced audio without the formant/quality artifacts of
// plain resampling. Pitch and time are handled independently: grain spacing
// scales with the pitch ratio, grain count with the time ratio.
//
// Pitch-mark detection is the fragile part, so this is conservative: it
// estimates a single average period by autocorrelation and only proceeds if the
// signal is reliably periodic (clear autocorrelation peak). If not, it reports
// failure (returns false) and the caller falls back to resample — better a
// known-OK result than PSOLA garbage on an unvoiced/noisy clip.

// Estimate fundamental period (in samples) via normalized autocorrelation.
// Returns 0 if no confident periodicity found.
size_t estimate_period(const std::vector<float>& s, int sr) {
    if (s.size() < 64) return 0;
    // search 70..400 Hz (typical voice range), clamped to signal length
    size_t min_lag = (size_t)std::max(2.0, sr / 400.0);
    size_t max_lag = (size_t)std::min<double>(sr / 70.0, s.size() / 2.0);
    if (max_lag <= min_lag) return 0;

    double energy = 0.0;
    for (float x : s) energy += (double)x * x;
    if (energy < 1e-9) return 0;

    double best_norm = 0.0; size_t best_lag = 0;
    for (size_t lag = min_lag; lag <= max_lag; ++lag) {
        double corr = 0.0, e2 = 0.0;
        for (size_t i = 0; i + lag < s.size(); ++i) {
            corr += (double)s[i] * s[i + lag];
            e2   += (double)s[i + lag] * s[i + lag];
        }
        double norm = (e2 > 1e-9) ? corr / std::sqrt(energy * e2) : 0.0;
        if (norm > best_norm) { best_norm = norm; best_lag = lag; }
    }
    // require a clear peak — below this, treat as unvoiced/aperiodic
    if (best_norm < 0.6) return 0;
    return best_lag;
}

// PSOLA time-stretch (the robust, phase-coherent operation): change duration
// while preserving pitch by repeating/skipping pitch-synchronous grains.
// Returns false if the clip isn't reliably periodic.
bool psola_timestretch(const std::vector<float>& in, int sr,
                       double time_ratio, std::vector<float>& out) {
    if (in.empty() || time_ratio <= 0.0) return false;
    size_t period = estimate_period(in, sr);
    if (period == 0) return false;

    std::vector<size_t> marks;
    for (size_t m = period; m + period < in.size(); m += period) marks.push_back(m);
    if (marks.size() < 2) return false;

    size_t grain_half = period;
    auto grain_at = [&](size_t center, std::vector<float>& g) {
        g.assign(2 * grain_half, 0.f);
        for (size_t i = 0; i < 2 * grain_half; ++i) {
            long src = (long)center - (long)grain_half + (long)i;
            if (src < 0 || src >= (long)in.size()) continue;
            double w = 0.5 - 0.5 * std::cos(2 * kPi * i / (2 * grain_half - 1));
            g[i] = (float)(in[src] * w);
        }
    };

    // synthesis keeps the SAME grain spacing (period) -> pitch unchanged; only
    // the number of grains changes with time_ratio. Phase stays coherent
    // because adjacent synthesis grains are adjacent analysis grains.
    size_t out_len = (size_t)(in.size() * time_ratio);
    out.assign(out_len, 0.f);
    std::vector<float> wsum(out_len, 0.f);
    std::vector<float> g;

    for (size_t syn_pos = 0; syn_pos < out_len; syn_pos += period) {
        double ana_pos = syn_pos / time_ratio;       // map back to analysis time
        size_t best = 0; double bestd = 1e18;
        for (size_t k = 0; k < marks.size(); ++k) {
            double d = std::fabs((double)marks[k] - ana_pos);
            if (d < bestd) { bestd = d; best = k; }
        }
        grain_at(marks[best], g);
        for (size_t i = 0; i < g.size(); ++i) {
            long dst = (long)syn_pos - (long)grain_half + (long)i;
            if (dst < 0 || dst >= (long)out_len) continue;
            double w = 0.5 - 0.5 * std::cos(2 * kPi * i / (2 * grain_half - 1));
            out[(size_t)dst]  += g[i];
            wsum[(size_t)dst] += (float)w;
        }
    }
    for (size_t i = 0; i < out_len; ++i)
        if (wsum[i] > 1e-6f) out[i] /= wsum[i];
    return true;
}

// PSOLA repitch+retime. pitch_ratio>1 raises pitch; time_ratio>1 lengthens.
// Pitch-shift = resample (moves pitch, changes length) then PSOLA time-stretch
// to restore the intended duration. This is the standard robust combination —
// it sidesteps the per-grain phase alignment that pure TD-PSOLA pitch-shift
// needs, and pairs with the separate formant stage for natural results.
// Returns false (leaving out untouched) if the clip isn't reliably periodic.
bool psola(const std::vector<float>& in, int sr,
           double pitch_ratio, double time_ratio,
           std::vector<float>& out) {
    if (in.empty() || pitch_ratio <= 0.0 || time_ratio <= 0.0) return false;
    if (estimate_period(in, sr) == 0) return false;  // not periodic -> fallback

    // final target length is set by time_ratio relative to the input
    size_t target_n = (size_t)(in.size() * time_ratio);
    if (target_n < 2) return false;

    std::vector<float> work = in;
    if (std::fabs(pitch_ratio - 1.0) > 1e-3) {
        // resample shifts pitch up by pitch_ratio and shortens by the same
        work = resample(work, pitch_ratio);
        if (work.size() < 4) return false;
    }
    // now restore (or set) duration to target_n via phase-coherent time-stretch
    double tr = (double)target_n / (double)work.size();
    std::vector<float> stretched;
    if (!psola_timestretch(work, sr, tr, stretched)) {
        // resampled signal may have shifted out of the voiced range; refit
        stretched = fit_to_length(work, target_n);
    }
    out = std::move(stretched);
    return true;
}

} // namespace dsp

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

        // formant shift FIRST (decoupled from pitch): moves the spectral
        // envelope while leaving length/pitch intact, so the following pitch
        // resample changes pitch without undoing the formant move.
        if (std::fabs(su.prosody.formant_shift) > 1e-3)
            s = dsp::formant_shift(s, su.prosody.formant_shift);

        // pitch + duration via PSOLA (artifact-free, independent control).
        // Falls back to the resample+refit path when the clip isn't reliably
        // periodic (PSOLA declines on unvoiced/noisy grains).
        double pitch_ratio = std::pow(2.0, su.prosody.pitch_offset_st / 12.0);
        size_t target_n = (size_t)((double)su.prosody.duration_ms * sr / 1000.0);
        bool need_pitch = std::fabs(su.prosody.pitch_offset_st) > 1e-3;
        bool need_time  = target_n > 0 && target_n != s.size();

        bool psola_ok = false;
        if (need_pitch || need_time) {
            double time_ratio = (target_n > 0) ? (double)target_n / (double)s.size() : 1.0;
            std::vector<float> ps;
            if (dsp::psola(s, sr, need_pitch ? pitch_ratio : 1.0,
                           need_time ? time_ratio : 1.0, ps) && !ps.empty()) {
                s = std::move(ps);
                psola_ok = true;
            }
        }
        if (!psola_ok) {
            // fallback: old resample (pitch+speed) then crude refit (duration)
            if (need_pitch) s = resample(s, pitch_ratio);
            if (target_n > 0) s = fit_to_length(s, target_n);
        }

        // sub-octave layer (chest/size) and rasp (grit) — character DSP
        if (su.prosody.sub_layer) s = dsp::add_sub_octave(s, 0.7);
        if (su.prosody.rasp > 1e-3) dsp::apply_rasp(s, su.prosody.rasp);

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
