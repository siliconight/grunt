#pragma once
// Generator.h — produces source clips for a voice bank from a license-cleared
// TTS generator (Piper first). The point is that "legally airtight" is
// structural: every generated clip is stamped with its model's license, and
// the ship gate refuses to bake anything not cleared. The generator binary is
// a BUILD-TIME tool; only its rendered audio (which is yours, per the model's
// CC0/permissive license) ends up in a shipped bank.
#include <functional>
#include "Types.h"
#include <string>
#include <vector>
#include <map>

namespace voc {

// One license-cleared model from the registry (data/voice_models.json).
struct VoiceModel {
    std::string id;
    std::string generator;     // "piper", "stub", ...
    std::string model_file;
    std::string license;       // "CC0-1.0", "Apache-2.0", ...
    bool commercial_use = false;
    bool redistributable = false;
    std::string source_url;
    std::string download_url;       // direct .onnx URL (empty = manual download)
    std::string download_url_json;  // direct .onnx.json URL
};

class VoiceModelRegistry {
public:
    bool load(const std::string& path, std::string& err);
    const VoiceModel* find(const std::string& id) const;
    const std::vector<VoiceModel>& all() const { return models_; }
private:
    std::vector<VoiceModel> models_;
};

// Result of generating one unit.
struct GeneratedClip {
    std::string key;           // the syllable/word this clip represents
    std::string wav_path;      // where the raw clip was written
    Provenance provenance;     // stamped from the model — what the gate reads
    bool ok = false;
    std::string error;
};

// Abstract generator backend. Piper shells out to the piper binary; Stub
// synthesizes a deterministic tone so the pipeline is testable offline.
// Detect how to invoke piper: GRUNT_PIPER_CMD if set, else the first working of
// {piper, python -m piper, py/python3 -m piper}, else "". Shared by generator,
// doctor, and GUI.
std::string detect_piper_cmd();

// Best-effort: get a working Piper onto this machine, no Python expertise needed.
// If Piper already works, returns its command immediately. Otherwise: finds an
// existing Python (py/python) or, on Windows with none, downloads + silently
// runs the official python.org per-user installer, then `pip install --user
// piper-tts`. Returns the working piper command, or "" on failure. `progress`
// (optional) receives human-readable status lines. WINDOWS-ONLY for the
// auto-download-Python step; on other OSes it only uses an existing Python.
std::string install_piper(const std::function<void(const std::string&)>& progress = {});

// Download a voice model (the .onnx and its .onnx.json) for `model` to the
// path resolve_dest(model.model_file) returns. Shared by the CLI fetch-voice
// command and the GUI's one-click download button so there's one code path.
// Returns true on success (or if already present). On failure, sets `err`.
// `progress` (optional) is called with human-readable status lines.
struct FetchOutcome { bool ok = false; bool already_present = false; std::string err; };
FetchOutcome fetch_voice_model(const VoiceModel& model,
                               const std::string& dest_onnx,
                               const std::function<void(const std::string&)>& progress = {});

class Generator {
public:
    virtual ~Generator() = default;
    // synthesize `text` for unit `key` into out_dir, return a stamped clip
    virtual GeneratedClip generate(const std::string& key,
                                   const std::string& text,
                                   const VoiceModel& model,
                                   const std::string& out_dir) = 0;
    virtual std::string name() const = 0;
};

// Factory: returns a backend by generator name, or nullptr if unknown.
Generator* make_generator(const std::string& name);

} // namespace voc
