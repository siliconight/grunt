#include "Stages.h"
#include <limits>
#include <unordered_map>
#include <cmath>
#include <vector>

namespace voc {

namespace {

// Candidate node in the lattice: a unit that could fill a slot, plus the tier
// it matched at (0 = exact word/syllable, higher = fallback).
struct Node {
    const AudioUnit* unit = nullptr;
    int tier = 0;
};

// Target cost: how well this unit fits the requested slot, independent of
// neighbours. Mirrors the old greedy per-unit cost.
double target_cost(const Node& n, Emotion want) {
    double c = 0.5 * n.tier;                       // fallback distance
    if (n.unit->type == UnitType::Grunt) c += 0.5; // mild grunt bias
    if (n.unit->emotion != want)         c += 0.5; // emotion mismatch
    return c;
}

} // namespace

namespace sel {
// Join cost: how well unit `a` flows into unit `b`. Penalizes pitch and energy
// discontinuity at the boundary (spectral/pitch discontinuity term, TDD §6.5),
// plus a strong penalty for repeating the exact same clip back-to-back.
double join_cost(const AudioUnit* a, const AudioUnit* b) {
    if (!a) return 0.0;                            // first slot: no join
    double c = 0.0;
    if (a->pitch_center_hz > 1.0 && b->pitch_center_hz > 1.0) {
        double semis = std::fabs(12.0 * std::log2(b->pitch_center_hz / a->pitch_center_hz));
        c += 0.15 * semis;                         // ~0.15 per semitone of jump
    }
    c += 0.8 * std::fabs(a->energy - b->energy);   // energy discontinuity
    if (a->id == b->id) c += 2.0;                  // immediate repetition
    return c;
}
} // namespace sel


std::vector<SelectedUnit> UnitSelector::select(const ProsodyPlan& pp,
                                               const UnitDatabase& db) {
    std::vector<SelectedUnit> out;
    if (pp.units.empty()) return out;

    // ---- build the lattice: candidate nodes per slot ----------------------
    std::vector<std::vector<Node>> lattice;
    lattice.reserve(pp.units.size());

    for (const auto& pu : pp.units) {
        std::vector<std::string> tiers;
        tiers.push_back(pu.key);
        for (const auto& f : pu.fallback) tiers.push_back(f);

        std::vector<Node> nodes;
        int matched_tier = -1;
        for (size_t t = 0; t < tiers.size(); ++t) {
            auto cands = db.match_key(tiers[t]);
            if (cands.empty()) continue;
            matched_tier = (int)t;
            for (const AudioUnit* u : cands) nodes.push_back({u, (int)t});
            break; // first matching tier supplies the candidates for this slot
        }
        if (matched_tier < 0) {
            auto grunts = db.match_key("");        // universal fallback
            for (const AudioUnit* u : grunts) nodes.push_back({u, (int)tiers.size()});
        }
        lattice.push_back(std::move(nodes));
    }

    bool any = false;
    for (auto& s : lattice) if (!s.empty()) { any = true; break; }
    if (!any) return out;

    // ---- Viterbi: min-cost path through the lattice -----------------------
    const double INF = std::numeric_limits<double>::max();
    size_t N = lattice.size();
    std::vector<std::vector<double>> dp(N);
    std::vector<std::vector<int>> bp(N);
    auto jitter = [&]() { return (double)(rng_() % 1000) / 1e6; };

    for (size_t i = 0; i < N; ++i) {
        dp[i].assign(lattice[i].size(), INF);
        bp[i].assign(lattice[i].size(), -1);
    }

    size_t first = 0;
    while (first < N && lattice[first].empty()) first++;
    if (first >= N) return out;

    for (size_t j = 0; j < lattice[first].size(); ++j)
        dp[first][j] = target_cost(lattice[first][j], pp.emotion) + jitter();

    size_t prev = first;
    for (size_t i = first + 1; i < N; ++i) {
        if (lattice[i].empty()) continue;
        for (size_t j = 0; j < lattice[i].size(); ++j) {
            double tc = target_cost(lattice[i][j], pp.emotion);
            double best = INF; int best_k = -1;
            for (size_t k = 0; k < lattice[prev].size(); ++k) {
                if (dp[prev][k] == INF) continue;
                double jc = sel::join_cost(lattice[prev][k].unit, lattice[i][j].unit);
                double cost = dp[prev][k] + tc + jc;
                if (cost < best) { best = cost; best_k = (int)k; }
            }
            if (best_k >= 0) { dp[i][j] = best + jitter(); bp[i][j] = best_k; }
        }
        prev = i;
    }

    // ---- backtrace the min-cost path --------------------------------------
    size_t last = prev;
    double best_end = INF; int best_j = -1;
    for (size_t j = 0; j < lattice[last].size(); ++j)
        if (dp[last][j] < best_end) { best_end = dp[last][j]; best_j = (int)j; }
    if (best_j < 0) return out;

    std::vector<size_t> slots;
    for (size_t i = first; i < N; ++i) if (!lattice[i].empty()) slots.push_back(i);

    std::vector<std::pair<size_t,int>> chosen;
    int node = best_j;
    for (size_t s = slots.size(); s-- > 0;) {
        size_t slot = slots[s];
        chosen.push_back({slot, node});
        if (s > 0 && node >= 0) node = bp[slot][node];
    }

    for (auto it = chosen.rbegin(); it != chosen.rend(); ++it) {
        size_t slot = it->first; int nd = it->second;
        if (nd < 0 || nd >= (int)lattice[slot].size()) continue;
        SelectedUnit su;
        su.unit = lattice[slot][nd].unit;
        su.prosody = pp.units[slot];
        out.push_back(su);
    }
    return out;
}

} // namespace voc
