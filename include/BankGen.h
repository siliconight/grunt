#pragma once
// BankGen — reusable "generate a voice bank from a CSV via a TTS generator"
// logic, shared by the CLI `generate` command and the GUI's Generate button so
// there's one code path, no drift.
#include <string>
#include <functional>

namespace voc {

struct GenerateOptions {
    std::string units_csv;       // path to key,text CSV
    std::string voice_dir;       // output bank dir
    std::string model_id;        // registry id
    std::string registry_path;   // data/voice_models.json (resolved)
    std::string generator;       // override; empty = model's default
    std::string unit_type = "word"; // "word" | "syllable"
};

struct GenerateResult {
    bool ok = false;
    int  units = 0;
    int  blocked = 0;            // units that won't pass the ship gate
    std::string error;           // set when ok == false
    std::string message;         // human-readable summary
};

// Generate a bank. `log` (optional) receives per-line progress/notes so a GUI
// can stream them; pass {} to ignore.
GenerateResult generate_bank(const GenerateOptions& opt,
                             const std::function<void(const std::string&)>& log = {});

} // namespace voc
