#include "Stages.h"
#include <limits>
#include <unordered_map>
#include <cmath>

namespace voc {

std::vector<SelectedUnit> UnitSelector::select(const ProsodyPlan& pp,
                                               const UnitDatabase& db) {
    std::vector<SelectedUnit> out;
    out.reserve(pp.units.size());

    // recent-use counts for repetition penalty (TDD §6.5)
    std::unordered_map<std::string, int> recent;

    for (const auto& pu : pp.units) {
        auto cands = db.candidates(pu.key, pp.emotion);
        if (cands.empty()) continue; // nothing to play for this unit

        const AudioUnit* best = nullptr;
        double best_cost = std::numeric_limits<double>::max();

        for (const AudioUnit* c : cands) {
            double cost = 0.0;

            // target cost: exact key match is cheapest; grunt fallback costs more
            if (c->key == pu.key)              cost += 0.0;
            else if (c->type == UnitType::Grunt) cost += 2.0;
            else                                 cost += 1.0;

            // emotion mismatch penalty
            if (c->emotion != pp.emotion)      cost += 0.5;

            // repetition penalty: discourage reusing the same clip id
            auto it = recent.find(c->id);
            if (it != recent.end()) cost += 1.5 * it->second;

            // tiny seeded jitter to break ties without bias (deterministic per seed)
            double jitter = (double)(rng_() % 1000) / 1e6;
            cost += jitter;

            if (cost < best_cost) { best_cost = cost; best = c; }
        }

        if (!best) continue;

        // decay all recent counts, bump chosen
        for (auto& kv : recent) if (kv.second > 0) kv.second--;
        recent[best->id] += 3;

        SelectedUnit su;
        su.unit = best;
        su.prosody = pu;
        out.push_back(su);
    }
    return out;
}

} // namespace voc
