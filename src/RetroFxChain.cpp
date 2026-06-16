#include "Stages.h"
#include <cmath>
#include <algorithm>

namespace voc {

namespace {
struct FxParams {
    double hp_hz;        // high-pass cutoff
    double lp_hz;        // low-pass cutoff
    double drive;        // saturation amount
    int    bits;         // bit-depth reduction target
    int    decimate;     // sample-rate reduction factor (sample-and-hold)
    double verb_mix;     // tiny room mix
};

FxParams params_for(const std::string& preset) {
    if (preset == "radio_ps1")   return { 400, 3000, 1.6, 8,  2, 0.05 };
    if (preset == "monster_ps1") return { 120, 4000, 2.2, 8,  3, 0.12 };
    if (preset == "robot_ps1")   return { 200, 5000, 2.8, 6,  2, 0.04 };
    if (preset == "muffled_mask")return { 150, 2200, 1.3, 10, 2, 0.08 };
    // clean_ps1 default
    return { 150, 6000, 1.4, 12, 2, 0.06 };
}

// one-pole filters
void high_pass(std::vector<float>& s, double cutoff, int sr) {
    double rc = 1.0 / (2.0 * kPi * cutoff);
    double dt = 1.0 / sr;
    double a = rc / (rc + dt);
    float prev_in = 0.f, prev_out = 0.f;
    for (float& x : s) {
        float out = (float)(a * (prev_out + x - prev_in));
        prev_in = x; prev_out = out; x = out;
    }
}
void low_pass(std::vector<float>& s, double cutoff, int sr) {
    double rc = 1.0 / (2.0 * kPi * cutoff);
    double dt = 1.0 / sr;
    double a = dt / (rc + dt);
    float prev = 0.f;
    for (float& x : s) { prev = (float)(prev + a * (x - prev)); x = prev; }
}
void saturate(std::vector<float>& s, double drive) {
    for (float& x : s) x = (float)std::tanh(x * drive) / (float)std::tanh(drive);
}
void bit_reduce(std::vector<float>& s, int bits) {
    float levels = (float)(1 << bits);
    for (float& x : s) x = std::round(x * levels) / levels;
}
void decimate(std::vector<float>& s, int factor) {
    if (factor <= 1) return;
    for (size_t i = 0; i < s.size(); ++i) s[i] = s[i - (i % factor)];
}
void tiny_verb(std::vector<float>& s, double mix, int sr) {
    size_t d = (size_t)(sr * 0.025); // 25 ms
    if (d == 0 || d >= s.size()) return;
    std::vector<float> wet = s;
    for (size_t i = d; i < s.size(); ++i) wet[i] += 0.5f * s[i - d];
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (float)((1.0 - mix) * s[i] + mix * wet[i]);
}
} // namespace

void RetroFxChain::apply(AudioBuffer& buf, const std::string& preset) const {
    if (buf.samples.empty()) return;
    FxParams p = params_for(preset);
    int sr = buf.sample_rate;

    high_pass(buf.samples, p.hp_hz, sr);
    low_pass(buf.samples, p.lp_hz, sr);
    saturate(buf.samples, p.drive);
    bit_reduce(buf.samples, p.bits);
    decimate(buf.samples, p.decimate);
    tiny_verb(buf.samples, p.verb_mix, sr);

    // final limiter (~ -1 dBFS)
    const float ceiling = 0.891f;
    float peak = 0.f;
    for (float x : buf.samples) peak = std::max(peak, std::fabs(x));
    if (peak > ceiling) {
        float g = ceiling / peak;
        for (float& x : buf.samples) x *= g;
    }
}

} // namespace voc
