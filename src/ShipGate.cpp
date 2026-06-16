#include "ShipGate.h"

namespace voc {

GateResult verify_bank(const UnitDatabase& db) {
    GateResult r;
    for (const auto& u : db.all()) {
        r.total++;
        const Provenance& p = u.provenance;

        if (p.synth_tool_derived) {
            r.failures.push_back(u.id + ": synth_tool_derived=true (eSpeak/MBROLA placeholder cannot ship)");
            continue;
        }
        if (!p.commercial_use) {
            r.failures.push_back(u.id + ": commercial_use=false");
            continue;
        }
        if (p.source == "sample_pack" &&
            (p.license.empty() || p.license == "owned")) {
            r.failures.push_back(u.id + ": source=sample_pack without a recognized commercial/redistributable license string");
            continue;
        }
        if (p.source == "synth_placeholder") {
            r.failures.push_back(u.id + ": source=synth_placeholder cannot ship");
            continue;
        }
    }
    r.passed = r.failures.empty();
    return r;
}

} // namespace voc
