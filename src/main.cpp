// grunt — build-time CLI that turns short game text into named PS1-style
// vocal clips from owned voice banks, and bakes a reproducible sound bank
// that Godot/gool imports and plays at runtime by name.
//
// Phase 0: grunt-vocalizer mode (TDD §12 Mode A, §18 Phase 0).

#include "Stages.h"
#include "Wav.h"
#include "ShipGate.h"
#include "AudioOut.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <filesystem>

using namespace voc;

namespace {

struct Args {
    std::map<std::string, std::string> opt;
    std::string get(const std::string& k, const std::string& def = "") const {
        auto it = opt.find(k); return it == opt.end() ? def : it->second;
    }
    bool has(const std::string& k) const { return opt.count(k) > 0; }
};

Args parse_args(int argc, char** argv, int start) {
    Args a;
    for (int i = start; i < argc; ++i) {
        std::string s = argv[i];
        if (s.rfind("--", 0) == 0) {
            std::string key = s.substr(2);
            if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
                a.opt[key] = argv[++i];
            else
                a.opt[key] = "1";
        }
    }
    return a;
}

// run the full pipeline for one line; returns rendered+FX'd buffer + stats
struct RenderStats { double peak_dbfs = 0; int units = 0; };

bool synth_line(const std::string& text, UnitDatabase& db,
                Emotion emotion, const std::string& fx,
                uint64_t seed, AudioBuffer& out, RenderStats& stats,
                std::string& err) {
    TextNormalizer norm;
    SyllablePlanner syl;
    ProsodyPlanner pros;
    UnitSelector sel(seed);
    AudioRenderer rend;
    RetroFxChain fxchain;

    NormalizedText nt = norm.normalize(text);
    if (db.all().empty()) { err = "empty bank"; return false; }

    UnitPlan up = syl.plan(nt);
    if (emotion != Emotion::Neutral) up.emotion = emotion; // CLI override
    ProsodyPlan pp = pros.plan(up);
    auto selected = sel.select(pp, db);
    stats.units = (int)selected.size();

    out = rend.render(selected, db);
    fxchain.apply(out, fx);
    stats.peak_dbfs = out.peak_dbfs();
    return true;
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

// Resolve output format. grunt defaults to OGG/Vorbis. If --format is omitted,
// OGG is used when this build supports it, otherwise it transparently falls
// back to WAV (with a one-line notice). Explicit --format ogg on a no-vorbis
// build is an error rather than a silent downgrade.
AudioFormat resolve_format(const Args& a, bool& notice_fallback) {
    notice_fallback = false;
    if (a.has("format")) return format_from_string(a.get("format"));
    if (ogg_supported()) return AudioFormat::Ogg;
    notice_fallback = true;
    return AudioFormat::Wav; // default would be OGG, but this build can't
}

float resolve_quality(const Args& a) {
    // libvorbis VBR quality, default 0.4 (decent for small game barks)
    return a.has("quality") ? std::stof(a.get("quality")) : 0.4f;
}

int cmd_synth(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    if (!a.has("text") || !a.has("voice") || !a.has("out")) {
        std::cerr << "usage: grunt synth --text \"...\" --voice <dir> --out <file>"
                     " [--emotion neutral|urgent|angry] [--style clean_ps1]"
                     " [--format ogg|wav] [--quality 0.4] [--seed N]\n"
                     "  default format: ogg (Vorbis). out extension is set to match the format.\n";
        return 2;
    }
    UnitDatabase db; std::string err;
    if (!db.load(a.get("voice"), err)) { std::cerr << "voice load failed: " << err << "\n"; return 1; }

    Emotion emo = a.has("emotion") ? emotion_from_string(a.get("emotion")) : Emotion::Neutral;
    std::string fx = a.get("style", "clean_ps1");
    uint64_t seed = a.has("seed") ? std::stoull(a.get("seed")) : 0x9E3779B97F4A7C15ULL;
    bool fb = false;
    AudioFormat fmt = resolve_format(a, fb);
    float q = resolve_quality(a);
    if (fb) std::cerr << "note: this build lacks libvorbis; writing WAV instead of OGG\n";
    if (fmt == AudioFormat::Ogg && !ogg_supported()) {
        std::cerr << "error: --format ogg requested but this build lacks libvorbis "
                     "(rebuild with -DGRUNT_HAVE_VORBIS, or use --format wav)\n";
        return 1;
    }

    AudioBuffer buf; RenderStats st;
    if (!synth_line(a.get("text"), db, emo, fx, seed, buf, st, err)) {
        std::cerr << "synth failed: " << err << "\n"; return 1;
    }

    // force the output path's extension to match the chosen format
    std::string out = a.get("out");
    {
        auto dot = out.find_last_of('.');
        auto slash = out.find_last_of("/\\");
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
            out = out.substr(0, dot);
        out += ".";
        out += format_ext(fmt);
    }

    if (!write_audio(out, buf, fmt, q, err)) { std::cerr << "write failed: " << err << "\n"; return 1; }

    std::cout << "wrote " << out
              << "  units=" << st.units
              << "  peak=" << st.peak_dbfs << " dBFS\n";
    return 0;
}

int cmd_verify(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    if (!a.has("voice")) { std::cerr << "usage: grunt verify --voice <dir>\n"; return 2; }
    UnitDatabase db; std::string err;
    if (!db.load(a.get("voice"), err)) { std::cerr << "voice load failed: " << err << "\n"; return 1; }

    GateResult r = verify_bank(db);
    std::cout << "ship gate: " << (r.passed ? "PASS" : "FAIL")
              << "  (" << r.total << " clips)\n";
    for (const auto& f : r.failures) std::cout << "  REJECT " << f << "\n";
    return r.passed ? 0 : 1;
}

// batch: CSV of name,voice,text,emotion[,fx] -> folder of named clips + manifest
int cmd_batch(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    if (!a.has("csv") || !a.has("voice") || !a.has("out-dir")) {
        std::cerr << "usage: grunt batch --csv <lines.csv> --voice <dir> --out-dir <dir>"
                     " [--style clean_ps1] [--format ogg|wav] [--quality 0.4] [--seed N]\n"
                     "  default format: ogg (Vorbis).\n";
        return 2;
    }
    UnitDatabase db; std::string err;
    if (!db.load(a.get("voice"), err)) { std::cerr << "voice load failed: " << err << "\n"; return 1; }

    // gate before producing anything (TDD §22)
    GateResult gate = verify_bank(db);
    if (!gate.passed) {
        std::cerr << "ship gate FAILED — refusing to bake bank:\n";
        for (const auto& f : gate.failures) std::cerr << "  REJECT " << f << "\n";
        return 1;
    }

    std::ifstream csv(a.get("csv"));
    if (!csv) { std::cerr << "cannot open csv: " << a.get("csv") << "\n"; return 1; }

    std::string fx = a.get("style", "clean_ps1");
    uint64_t seed = a.has("seed") ? std::stoull(a.get("seed")) : 0x9E3779B97F4A7C15ULL;
    bool fb = false;
    AudioFormat fmt = resolve_format(a, fb);
    float q = resolve_quality(a);
    if (fb) std::cerr << "note: this build lacks libvorbis; writing WAV instead of OGG\n";
    if (fmt == AudioFormat::Ogg && !ogg_supported()) {
        std::cerr << "error: --format ogg requested but this build lacks libvorbis "
                     "(rebuild with -DGRUNT_HAVE_VORBIS, or use --format wav)\n";
        return 1;
    }
    const char* ext = format_ext(fmt);
    std::string out_dir = a.get("out-dir");
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) { std::cerr << "cannot create out-dir " << out_dir << ": " << ec.message() << "\n"; return 1; }

    std::ostringstream manifest;
    manifest << "{\n  \"bank_id\": \"" << json_escape(db.voice_id()) << "_vo\",\n"
             << "  \"schema_version\": 1,\n  \"clips\": [\n";

    std::string line; bool first = true; int n = 0; bool header_skipped = false;
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        // split CSV (no quoted-comma support in Phase 0)
        std::vector<std::string> f; std::stringstream ss(line); std::string cell;
        while (std::getline(ss, cell, ',')) f.push_back(cell);
        if (f.size() < 3) continue;
        if (!header_skipped && f[0] == "name") { header_skipped = true; continue; }
        header_skipped = true;

        std::string name = f[0], text = f[2];
        Emotion emo = f.size() > 3 ? emotion_from_string(f[3]) : Emotion::Neutral;
        std::string clip_fx = f.size() > 4 && !f[4].empty() ? f[4] : fx;
        // per-clip deterministic seed derived from base seed + name
        uint64_t cseed = seed;
        for (char c : name) cseed = cseed * 1099511628211ULL + (unsigned char)c;

        AudioBuffer buf; RenderStats st;
        if (!synth_line(text, db, emo, clip_fx, cseed, buf, st, err)) {
            std::cerr << "  skip " << name << ": " << err << "\n"; continue;
        }
        std::string rel = name + "." + ext;
        std::string path = out_dir + "/" + rel;
        if (!write_audio(path, buf, fmt, q, err)) { std::cerr << "  write fail " << name << ": " << err << "\n"; continue; }

        if (!first) manifest << ",\n";
        first = false;
        int dur_ms = (int)(buf.samples.size() * 1000.0 / buf.sample_rate);
        manifest << "    { \"name\": \"" << json_escape(name) << "\""
                 << ", \"file\": \"" << json_escape(rel) << "\""
                 << ", \"voice_id\": \"" << json_escape(db.voice_id()) << "\""
                 << ", \"text\": \"" << json_escape(text) << "\""
                 << ", \"emotion\": \"" << emotion_to_string(emo) << "\""
                 << ", \"fx_preset\": \"" << json_escape(clip_fx) << "\""
                 << ", \"duration_ms\": " << dur_ms
                 << ", \"sample_rate\": " << buf.sample_rate
                 << ", \"peak_dbfs\": " << st.peak_dbfs
                 << ", \"seed\": " << cseed << " }";
        n++;
    }
    manifest << "\n  ]\n}\n";

    std::ofstream mf(out_dir + "/bank.json");
    mf << manifest.str();
    std::cout << "baked " << n << " clips -> " << out_dir << "  (+ bank.json)\n";
    return 0;
}

int cmd_phonemes(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    if (!a.has("text")) { std::cerr << "usage: grunt phonemes --text \"...\"\n"; return 2; }
    // Phase 0 stub: show the grunt-mode unit plan instead of true phonemes.
    TextNormalizer norm; SyllablePlanner syl;
    NormalizedText nt = norm.normalize(a.get("text"));
    UnitPlan up = syl.plan(nt);
    std::cout << "emotion=" << emotion_to_string(nt.emotion_hint)
              << " punct='" << nt.terminal_punct << "'\nunits:";
    for (auto& u : up.units) std::cout << " " << u.key << (u.is_emphasis ? "*" : "");
    std::cout << "\n(note: Phase 0 grunt-mode units; true phoneme view lands in Phase 1)\n";
    return 0;
}

void usage() {
    std::cout <<
    "grunt — PS1-style game vocalizer (Phase 0)\n\n"
    "commands:\n"
    "  synth     render one line to a WAV\n"
    "  batch     bake a folder of named clips + bank.json from a CSV\n"
    "  verify    run the provenance ship gate on a voice bank\n"
    "  phonemes  show the grunt-mode unit plan for a line (debug)\n\n"
    "run a command with no args for its usage.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }
    std::string cmd = argv[1];
    if (cmd == "synth")    return cmd_synth(argc, argv);
    if (cmd == "batch")    return cmd_batch(argc, argv);
    if (cmd == "verify")   return cmd_verify(argc, argv);
    if (cmd == "phonemes") return cmd_phonemes(argc, argv);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }
    std::cerr << "unknown command: " << cmd << "\n";
    usage();
    return 2;
}
