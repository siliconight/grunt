#pragma once
// ShipGate.h — the provenance ship gate (TDD §22). A bank cannot package
// unless every clip traces to a clear commercial-use right.
#include "Stages.h"
#include <string>
#include <vector>

namespace voc {

struct GateResult {
    bool passed = true;
    std::vector<std::string> failures; // human-readable per-clip reasons
    int total = 0;
};

GateResult verify_bank(const UnitDatabase& db);

} // namespace voc
