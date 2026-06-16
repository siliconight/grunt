#include "Stages.h"

namespace voc {

namespace {
struct EmotionProfile {
    double speed_mult;     // <1 slower, >1 faster
    double pitch_slope_st; // total semitone drift across the line
    double base_gain_db;
    double stress_dur_mult;
    double stress_gain_db;
};

EmotionProfile profile_for(Emotion e) {
    switch (e) {
        // angry: faster, falling contour, hot and punchy
        case Emotion::Angry:  return { 1.15, -2.0, -2.0, 1.35, +3.0 };
        // urgent: faster, slight rise, forward
        case Emotion::Urgent: return { 1.20, +1.0, -2.5, 1.25, +2.5 };
        // neutral: gentle declination
        default:              return { 1.00, -1.0, -3.0, 1.20, +2.0 };
    }
}
} // namespace

ProsodyPlan ProsodyPlanner::plan(const UnitPlan& up) const {
    ProsodyPlan pp;
    pp.emotion = up.emotion;
    const EmotionProfile prof = profile_for(up.emotion);

    const size_t n = up.units.size();
    const int base_dur_ms = 140;

    for (size_t i = 0; i < n; ++i) {
        const RequestedUnit& ru = up.units[i];
        ProsodyUnit u;
        u.key = ru.key;
        u.stress = ru.is_emphasis;

        // declination: linear pitch drift across the whole line
        double t = (n > 1) ? (double)i / (double)(n - 1) : 0.0;
        u.pitch_offset_st = prof.pitch_slope_st * t;

        // duration: base / speed, lengthen stressed + final unit
        double dur = base_dur_ms / prof.speed_mult;
        if (u.stress) dur *= prof.stress_dur_mult;
        bool final_unit = (i + 1 == n);
        if (final_unit && (up.terminal_punct == "." || up.terminal_punct.empty()))
            dur *= 1.3; // phrase-final lengthening on statements
        u.duration_ms = (int)dur;

        // question: lift the final unit's pitch
        if (final_unit && (up.terminal_punct == "?" || up.terminal_punct == "?!"))
            u.pitch_offset_st += 2.0;

        u.gain_db = prof.base_gain_db + (u.stress ? prof.stress_gain_db : 0.0);
        pp.units.push_back(u);
    }
    return pp;
}

} // namespace voc
