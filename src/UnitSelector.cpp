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
        // Build the ordered tier list: primary key first, then fallbacks
        // (phonemes, then "" = grunt). Walk until a tier has matches.
        std::vector<std::string> tiers;
        tiers.push_back(pu.key);
        for (const auto& f : pu.fallback) tiers.push_back(f);

        std::vector<const AudioUnit*> cands;
        int tier_index = 0;
        for (size_t t = 0; t < tiers.size(); ++t) {
            cands = db.match_key(tiers[t]);
            if (!cands.empty()) { tier_index = (int)t; break; }
        }
        if (cands.empty()) {
            // last resort: any grunt (covers banks lacking the requested keys)
            cands = db.match_key("");
            tier_index = (int)tiers.size();
        }
        if (cands.empty()) continue; // bank has no grunts either; skip

        const AudioUnit* best = nullptr;
        double best_cost = std::numeric_limits<double>::max();

        for (const AudioUnit* c : cands) {
            // tier cost: earlier tier (closer to the intended syllable) is cheaper
            double cost = 0.5 * tier_index;
            if (c->type == UnitType::Grunt) cost += 0.5; // mild grunt bias

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
