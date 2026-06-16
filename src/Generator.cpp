#include "Generator.h"
#include "Json.h"
#include "Wav.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdlib>

namespace voc {

// ---- registry --------------------------------------------------------------
bool VoiceModelRegistry::load(const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open voice model registry: " + path; return false; }
    std::stringstream ss; ss << f.rdbuf();
    try {
        Json root = Json::parse(ss.str());
        const Json& list = root["models"];
        for (const auto& m : list.items()) {
            VoiceModel vm;
            vm.id            = m["id"].as_string();
            vm.generator     = m["generator"].as_string();
            vm.model_file    = m["model_file"].as_string();
            vm.license       = m["license"].as_string();
            vm.commercial_use = m["commercial_use"].as_bool(false);
            vm.redistributable = m["redistributable"].as_bool(false);
            vm.source_url    = m["source_url"].as_string();
            vm.download_url      = m["download_url"].as_string();
            vm.download_url_json = m["download_url_json"].as_string();
            models_.push_back(std::move(vm));
        }
    } catch (const std::exception& e) {
        err = std::string("registry parse: ") + e.what(); return false;
    }
    return true;
}

const VoiceModel* VoiceModelRegistry::find(const std::string& id) const {
    for (const auto& m : models_) if (m.id == id) return &m;
    return nullptr;
}

// ---- provenance stamping ---------------------------------------------------
// The model's license becomes the clip's provenance. A CC0/permissive model
// with commercial+redistributable rights yields a shippable clip; anything
// else is recorded honestly so the gate can reject it.
static Provenance stamp(const VoiceModel& m, const std::string& gen_name) {
    Provenance p;
    p.source = "generated_tts";
    p.recorded_by = gen_name + ":" + m.id;
    p.license = m.license;
    p.commercial_use = m.commercial_use && m.redistributable;
    // generated TTS from a cleared model is NOT a synth_placeholder; it's a
    // real shippable unit IF the model is commercial+redistributable.
    p.synth_tool_derived = false;
    return p;
}

// ---- Piper backend ---------------------------------------------------------
// Shells out to the `piper` binary (build-time tool). grunt never links Piper.
class PiperGenerator : public Generator {
public:
    std::string name() const override { return "piper"; }

    GeneratedClip generate(const std::string& key, const std::string& text,
                           const VoiceModel& model, const std::string& out_dir) override {
        GeneratedClip c;
        c.key = key;
        c.wav_path = out_dir + "/" + key + ".wav";
        c.provenance = stamp(model, name());

        // piper reads text on stdin, writes a WAV to --output_file.
        // The piper command is configurable via GRUNT_PIPER_CMD so the same
        // code works with: the modern Python CLI ("python -m piper"), a
        // standalone piper.exe, or a bundled binary path. Defaults to "piper".
        const char* env = std::getenv("GRUNT_PIPER_CMD");
        std::string piper_cmd = (env && *env) ? env : "piper";

        std::ostringstream cmd;
        cmd << "echo " << shell_quote(text)
            << " | " << piper_cmd << " --model " << shell_quote(model.model_file)
            << " --output_file " << shell_quote(c.wav_path);
        int rc = std::system(cmd.str().c_str());
        if (rc != 0) {
            c.error = "piper invocation failed (rc=" + std::to_string(rc) +
                      "); ensure piper is installed (pip install piper-tts) and the model exists";
            return c;
        }
        // sanity: confirm the file is a readable WAV
        AudioBuffer probe; std::string err;
        if (!read_wav(c.wav_path, probe, err)) {
            c.error = "piper produced no readable WAV: " + err;
            return c;
        }
        c.ok = true;
        return c;
    }

private:
    static std::string shell_quote(const std::string& s) {
        std::string o = "'";
        for (char ch : s) { if (ch == '\'') o += "'\\''"; else o += ch; }
        o += "'";
        return o;
    }
};

// ---- Stub backend ----------------------------------------------------------
// Deterministic synthetic tone so the whole pipeline is testable offline with
// no Piper install. Clips it produces are marked synth_tool_derived=true so the
// ship gate ALWAYS rejects them — a stub clip can never leak into a real bank.
class StubGenerator : public Generator {
public:
    std::string name() const override { return "stub"; }

    GeneratedClip generate(const std::string& key, const std::string& text,
                           const VoiceModel& model, const std::string& out_dir) override {
        GeneratedClip c;
        c.key = key;
        c.wav_path = out_dir + "/" + key + ".wav";
        c.provenance = stamp(model, name());
        c.provenance.synth_tool_derived = true; // stub output is never shippable

        // length/pitch derived deterministically from the key so it's stable
        AudioBuffer b; b.sample_rate = 22050;
        uint32_t h = 2166136261u;
        for (char ch : key) h = (h ^ (unsigned char)ch) * 16777619u;
        double f0 = 90.0 + (h % 120);
        double dur = 0.12 + ((h >> 8) % 12) * 0.01;
        int n = (int)(dur * b.sample_rate);
        for (int i = 0; i < n; ++i) {
            double t = (double)i / b.sample_rate;
            double env = std::sin(kPi * (double)i / n);
            b.samples.push_back((float)(0.6 * env * std::sin(2 * kPi * f0 * t)));
        }
        std::string err;
        if (!write_wav(c.wav_path, b, err)) { c.error = err; return c; }
        (void)text; (void)model;
        c.ok = true;
        return c;
    }
};

Generator* make_generator(const std::string& name) {
    if (name == "piper") return new PiperGenerator();
    if (name == "stub")  return new StubGenerator();
    return nullptr;
}

} // namespace voc
