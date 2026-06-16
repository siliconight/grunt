// grunt — build-time CLI that turns short game text into named PS1-style
// vocal clips from owned voice banks, and bakes a reproducible sound bank
// that Godot/gool imports and plays at runtime by name.
//
// Phase 0: grunt-vocalizer mode (TDD §12 Mode A, §18 Phase 0).

#include "Stages.h"
#include "Wav.h"
#include "ShipGate.h"
#include "AudioOut.h"
#include "Engine.h"
#include "Generator.h"
#include "Character.h"
#include "Vocalization.h"
#include "ResourcePath.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
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

// All synthesis goes through the shared Engine (one pipeline, no drift).

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
    bool has_input = a.has("text") || a.has("effort") || a.has("onomatopoeia");
    if (!has_input || !a.has("voice") || !a.has("out")) {
        std::cerr << "usage: grunt synth (--text \"...\" | --effort <id> | --onomatopoeia \"aaargh\")"
                     " --voice <dir> --out <file>"
                     " [--character <name>] [--emotion neutral|urgent|angry]"
                     " [--style clean_ps1] [--format ogg|wav] [--quality 0.4] [--seed N]\n"
                     "  --effort renders a named vocalization from data/efforts.json (pain_death, yell, ...).\n"
                     "  --onomatopoeia voices a literal spelling as a sound (repeated letters = more intense).\n"
                     "  --character applies a preset from data/characters.json (overrides emotion/style).\n"
                     "  default format: ogg (Vorbis). out extension is set to match the format.\n";
        return 2;
    }
    std::string err;

    Emotion emo = a.has("emotion") ? emotion_from_string(a.get("emotion")) : Emotion::Neutral;
    std::string fx = a.get("style", "clean_ps1");
    uint64_t seed = a.has("seed") ? std::stoull(a.get("seed")) : 0x9E3779B97F4A7C15ULL;
    Engine::Options opts;

    // --character applies a preset recipe (overrides emotion/style unless those
    // were explicitly given). Pitch/gain layer onto the render.
    if (a.has("character")) {
        CharacterLibrary lib;
        std::string cpath = a.get("characters", resource_path("data/characters.json"));
        if (!lib.load(cpath, err)) { std::cerr << "characters: " << err << "\n"; return 1; }
        const CharacterPreset* cp = lib.find(a.get("character"));
        if (!cp) {
            std::cerr << "character '" << a.get("character") << "' not found in " << cpath << "\n"
                      << "available characters:\n";
            for (const auto& p : lib.all())
                std::cerr << "  " << p.id << "  (" << p.display_name << ")"
                          << (p.ready ? "" : "  [needs DSP: " + p.blocked_on + "]") << "\n";
            return 1;
        }
        if (!cp->ready)
            std::cerr << "note: character '" << cp->id << "' needs DSP not yet built ("
                      << cp->blocked_on << "); rendering a reasonable approximation.\n";
        if (!a.has("style"))   fx = cp->fx_preset;
        if (!a.has("emotion")) emo = emotion_from_string(cp->emotion_bias);
        opts.extra_pitch_st = cp->pitch_offset_st;
        opts.extra_gain_db  = cp->gain_db;
        opts.formant_shift  = cp->formant_shift;
        opts.sub_layer      = cp->sub_layer;
        opts.rasp           = cp->rasp ? 0.6 : 0.0;
    }

    bool fb = false;
    AudioFormat fmt = resolve_format(a, fb);
    float q = resolve_quality(a);
    if (fb) std::cerr << "note: this build lacks libvorbis; writing WAV instead of OGG\n";
    if (fmt == AudioFormat::Ogg && !ogg_supported()) {
        std::cerr << "error: --format ogg requested but this build lacks libvorbis "
                     "(rebuild with -DGRUNT_HAVE_VORBIS, or use --format wav)\n";
        return 1;
    }

    Engine engine;
    if (!engine.load_voice(a.get("voice"), err)) { std::cerr << "voice load failed: " << err << "\n"; return 1; }

    SynthResult res;
    if (a.has("effort")) {
        EffortLibrary elib;
        std::string epath = a.get("efforts", resource_path("data/efforts.json"));
        if (!elib.load(epath, err)) { std::cerr << "efforts: " << err << "\n"; return 1; }
        const Effort* ef = elib.find(a.get("effort"));
        if (!ef) {
            std::cerr << "effort '" << a.get("effort") << "' not found in " << epath << "\n"
                      << "available efforts:\n";
            for (const auto& e : elib.all())
                std::cerr << "  " << e.id << "  — " << e.desc << "\n";
            return 1;
        }
        PhonemeSeq seq = effort_to_phonemes(*ef);
        if (!a.has("emotion")) emo = ef->emotion;
        seq.emotion = emo;
        res = engine.synth_vocalization(seq, ef->intensity, fx, seed, opts);
    } else if (a.has("onomatopoeia")) {
        PhonemeMapper mapper;  // letter-level G2P; no dict needed for onomatopoeia
        double intensity = 0.7;
        PhonemeSeq seq = onomatopoeia_to_phonemes(a.get("onomatopoeia"), mapper, intensity);
        if (a.has("emotion")) seq.emotion = emo;
        res = engine.synth_vocalization(seq, intensity, fx, seed, opts);
    } else {
        res = engine.synth(a.get("text"), emo, fx, seed, opts);
    }
    if (!res.ok) { std::cerr << "synth failed: " << res.error << "\n"; return 1; }
    AudioBuffer& buf = res.audio;

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
              << "  units=" << res.units
              << "  peak=" << res.peak_dbfs << " dBFS\n";
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
    Engine engine; std::string err;
    if (!engine.load_voice(a.get("voice"), err)) { std::cerr << "voice load failed: " << err << "\n"; return 1; }

    // gate before producing anything (TDD §22)
    GateResult gate = verify_bank(engine.db());
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
    manifest << "{\n  \"bank_id\": \"" << json_escape(engine.voice_id()) << "_vo\",\n"
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

        SynthResult res = engine.synth(text, emo, clip_fx, cseed);
        if (!res.ok) {
            std::cerr << "  skip " << name << ": " << res.error << "\n"; continue;
        }
        AudioBuffer& buf = res.audio;
        std::string rel = name + "." + ext;
        std::string path = out_dir + "/" + rel;
        if (!write_audio(path, buf, fmt, q, err)) { std::cerr << "  write fail " << name << ": " << err << "\n"; continue; }

        if (!first) manifest << ",\n";
        first = false;
        int dur_ms = (int)(buf.samples.size() * 1000.0 / buf.sample_rate);
        manifest << "    { \"name\": \"" << json_escape(name) << "\""
                 << ", \"file\": \"" << json_escape(rel) << "\""
                 << ", \"voice_id\": \"" << json_escape(engine.voice_id()) << "\""
                 << ", \"text\": \"" << json_escape(text) << "\""
                 << ", \"emotion\": \"" << emotion_to_string(emo) << "\""
                 << ", \"fx_preset\": \"" << json_escape(clip_fx) << "\""
                 << ", \"duration_ms\": " << dur_ms
                 << ", \"sample_rate\": " << buf.sample_rate
                 << ", \"peak_dbfs\": " << res.peak_dbfs
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
    if (!a.has("text")) {
        std::cerr << "usage: grunt phonemes --text \"...\" [--dict cmudict.dict]\n";
        return 2;
    }
    TextNormalizer norm;
    PhonemeMapper mapper;
    if (a.has("dict")) {
        std::string err;
        if (!mapper.load_dictionary(a.get("dict"), err)) {
            std::cerr << "dictionary load failed: " << err << "\n"
                      << "(continuing with rule-based fallback for all words)\n";
        } else {
            std::cerr << "loaded " << mapper.dictionary_size() << " dictionary entries\n";
        }
    }

    NormalizedText nt = norm.normalize(a.get("text"));
    PhonemeSeq seq = mapper.map(nt);

    std::cout << "emotion=" << emotion_to_string(seq.emotion)
              << " punct='" << seq.terminal_punct << "'\n";

    // ARPAbet view: words separated by |
    std::vector<std::string> unknown;
    bool first = true;
    for (const auto& w : seq.words) {
        std::cout << (first ? "" : " | ");
        first = false;
        for (size_t i = 0; i < w.phonemes.size(); ++i)
            std::cout << (i ? " " : "") << w.phonemes[i];
        if (w.is_emphasis) std::cout << " (*)";
        if (w.source == PhonemeSource::RuleFallback) unknown.push_back(w.word);
    }
    std::cout << "\n";

    if (!unknown.empty()) {
        std::cout << "unknown words (rule fallback): ";
        for (size_t i = 0; i < unknown.size(); ++i)
            std::cout << (i ? ", " : "") << unknown[i];
        std::cout << "\n";
    }
    if (!a.has("dict"))
        std::cout << "(no --dict given; all words used the rule-based fallback)\n";
    return 0;
}

// generate — build a voice bank's units from a CC0/permissive TTS generator.
// Reads a units CSV (key,text), synthesizes each unit, writes clips +
// units.json with provenance stamped from the registry-cleared model.
int cmd_generate(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    if (!a.has("units") || !a.has("voice") || !a.has("model")) {
        std::cerr << "usage: grunt generate --units <units.csv> --voice <dir> --model <model-id>\n"
                     "  [--generator piper|stub] [--registry data/voice_models.json]\n"
                     "  units.csv rows: key,text  (e.g.  ge,gah)\n"
                     "  --model must be a license-cleared id from the registry.\n";
        return 2;
    }
    std::string registry_path = a.get("registry", resource_path("data/voice_models.json"));
    VoiceModelRegistry reg; std::string err;
    if (!reg.load(registry_path, err)) { std::cerr << "registry: " << err << "\n"; return 1; }

    const VoiceModel* model = reg.find(a.get("model"));
    if (!model) {
        std::cerr << "model '" << a.get("model") << "' not found in registry " << registry_path << "\n"
                  << "available cleared models:\n";
        for (const auto& m : reg.all())
            std::cerr << "  " << m.id << "  (" << m.license << ", "
                      << (m.commercial_use && m.redistributable ? "shippable" : "NOT shippable") << ")\n";
        return 1;
    }

    std::string gen_name = a.get("generator", model->generator);
    Generator* gen = make_generator(gen_name);
    if (!gen) { std::cerr << "unknown generator: " << gen_name << "\n"; return 1; }

    std::string voice_dir = a.get("voice");
    std::string units_dir = voice_dir + "/units/generated";
    std::error_code ec;
    std::filesystem::create_directories(units_dir, ec);
    if (ec) { std::cerr << "cannot create " << units_dir << ": " << ec.message() << "\n"; delete gen; return 1; }

    std::ifstream csv(a.get("units"));
    if (!csv) { std::cerr << "cannot open units csv: " << a.get("units") << "\n"; delete gen; return 1; }

    std::ostringstream units_json;
    units_json << "{\n  \"units\": [\n";

    std::string line; bool first = true; int n = 0, blocked = 0; bool hdr = false;
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        std::vector<std::string> f; std::stringstream ss(line); std::string cell;
        while (std::getline(ss, cell, ',')) f.push_back(cell);
        if (f.size() < 2) continue;
        if (!hdr && f[0] == "key") { hdr = true; continue; }
        hdr = true;

        std::string key = f[0], text = f[1];
        GeneratedClip clip = gen->generate(key, text, *model, units_dir);
        if (!clip.ok) { std::cerr << "  skip " << key << ": " << clip.error << "\n"; continue; }

        // honest provenance: warn if this clip won't pass the gate
        if (!clip.provenance.commercial_use || clip.provenance.synth_tool_derived) {
            std::cerr << "  note: " << key << " is not shippable ("
                      << (clip.provenance.synth_tool_derived ? "stub/synth-derived" : "model not commercial+redistributable")
                      << ") — gate will block it\n";
            blocked++;
        }

        // Bake as a WORD unit keyed by the spoken text (lowercased, first word
        // if multi-word), so the planner's word-first lookup matches when a
        // user types that word. Single-word texts give the cleanest match;
        // multi-word texts (e.g. "over there") are keyed by the whole phrase
        // AND remain reachable as a bark by the CSV key via the id.
        std::string wordkey = text;
        for (auto& c : wordkey) c = (char)std::tolower((unsigned char)c);
        // trim leading/trailing spaces
        while (!wordkey.empty() && wordkey.front() == ' ') wordkey.erase(wordkey.begin());
        while (!wordkey.empty() && wordkey.back() == ' ') wordkey.pop_back();

        std::string rel = "units/generated/" + key + ".wav";
        if (!first) units_json << ",\n";
        first = false;
        units_json << "    { \"id\": \"gen_" << json_escape(key) << "\""
                   << ", \"type\": \"word\", \"key\": \"" << json_escape(wordkey) << "\""
                   << ", \"emotion\": \"neutral\", \"file\": \"" << json_escape(rel) << "\""
                   << ", \"provenance\": {"
                   << " \"source\": \"" << json_escape(clip.provenance.source) << "\""
                   << ", \"recorded_by\": \"" << json_escape(clip.provenance.recorded_by) << "\""
                   << ", \"license\": \"" << json_escape(clip.provenance.license) << "\""
                   << ", \"commercial_use\": " << (clip.provenance.commercial_use ? "true" : "false")
                   << ", \"synth_tool_derived\": " << (clip.provenance.synth_tool_derived ? "true" : "false")
                   << " } }";
        n++;
    }
    units_json << "\n  ]\n}\n";

    std::filesystem::create_directories(voice_dir + "/metadata", ec);
    std::ofstream mf(voice_dir + "/metadata/units.json");
    mf << units_json.str();

    delete gen;
    std::cout << "generated " << n << " units via " << gen_name
              << " (model " << model->id << ", " << model->license << ") -> " << units_dir << "\n";
    if (blocked)
        std::cout << blocked << " unit(s) are NOT shippable and the ship gate will block them.\n";
    else
        std::cout << "all units passed provenance; bank is gate-clean.\n";
    return 0;
}

// grunt version — bump with each tagged release.
static const char* kGruntVersion = "0.10.0";

// Locate the bundled demo bank relative to either the CWD or the executable's
// repo layout, so `grunt quickstart` works from a build dir or the repo root.
std::string find_demo_bank() {
    std::error_code ec;
    std::string p = resource_path("voices/heavy_brother");
    if (std::filesystem::exists(p + "/voice.json", ec)) return p;
    // extra CWD-relative fallbacks for unusual build layouts
    for (const char* c : {"voices/heavy_brother", "../voices/heavy_brother",
                          "../../voices/heavy_brother"}) {
        if (std::filesystem::exists(std::string(c) + "/voice.json", ec)) return c;
    }
    return "";
}

// quickstart — zero-config first run: render a few demo clips from the bundled
// bank so a new user hears grunt in ONE command, before any Piper/model setup.
int cmd_quickstart(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    std::string out_dir = a.get("out", "grunt_quickstart");
    std::string bank = find_demo_bank();
    if (bank.empty()) {
        std::cerr << "couldn't find the bundled demo bank (voices/heavy_brother).\n"
                     "run this from the grunt repo root, or pass --voice <dir>.\n";
        return 1;
    }
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);

    // WAV by default for the demo so it works even without libvorbis.
    bool ogg = ogg_supported() && !a.has("wav");
    AudioFormat fmt = ogg ? AudioFormat::Ogg : AudioFormat::Wav;
    const char* ext = ogg ? "ogg" : "wav";
    float q = 0.5f;
    std::string err;

    Engine engine;
    if (!engine.load_voice(bank, err)) {
        std::cerr << "demo bank failed to load: " << err << "\n"; return 1;
    }

    struct Demo { const char* name; const char* text; const char* character; const char* effort; };
    Demo demos[] = {
        {"01_hello",        "hello there",   nullptr,        nullptr},
        {"02_grunt",        "who goes there", "grunt",        nullptr},
        {"03_robot",        "scanning area",  "robot",        nullptr},
        {"04_guard_death",  nullptr,          "yelling_man",  "pain_death"},
        {"05_orc_roar",     nullptr,          "orc",          "yell"},
    };

    std::cout << "grunt quickstart — rendering demo clips from the bundled bank\n"
              << "(no Piper or downloads needed; this is the zero-config path)\n\n";

    int made = 0;
    for (const auto& d : demos) {
        std::string out = out_dir + "/" + d.name + "." + ext;
        SynthResult res;
        Engine::Options opts;
        std::string fx = "clean_ps1";
        Emotion emo = Emotion::Neutral;

        // apply character preset if named
        if (d.character) {
            CharacterLibrary lib;
            if (lib.load(resource_path("data/characters.json"), err)) {
                if (const CharacterPreset* cp = lib.find(d.character)) {
                    fx = cp->fx_preset;
                    emo = emotion_from_string(cp->emotion_bias);
                    opts.extra_pitch_st = cp->pitch_offset_st;
                    opts.extra_gain_db  = cp->gain_db;
                    opts.formant_shift  = cp->formant_shift;
                    opts.sub_layer      = cp->sub_layer;
                    opts.rasp           = cp->rasp ? 0.6 : 0.0;
                }
            }
        }

        if (d.effort) {
            EffortLibrary el;
            if (el.load(resource_path("data/efforts.json"), err)) {
                if (const Effort* ef = el.find(d.effort)) {
                    PhonemeSeq seq = effort_to_phonemes(*ef);
                    res = engine.synth_vocalization(seq, ef->intensity, fx, 0xC0FFEE, opts);
                }
            }
        } else {
            res = engine.synth(d.text, emo, fx, 0xC0FFEE, opts);
        }

        if (res.ok) {
            AudioFormat use = fmt;
            if (!write_audio(out, res.audio, use, q, err)) {
                std::cerr << "  ! " << d.name << ": " << err << "\n";
            } else {
                std::cout << "  ✓ " << out
                          << (d.character ? std::string("  [") + d.character + "]" : "")
                          << (d.effort ? std::string(" effort:") + d.effort : "") << "\n";
                made++;
            }
        }
    }

    std::cout << "\n" << made << " clips in " << out_dir << "/  — play them!\n\n"
              << "next steps:\n"
              << "  • one line, your text:   grunt synth --text \"take cover\" --character grunt --voice "
              << bank << " --out test." << ext << "\n"
              << "  • an effort/scream:      grunt synth --effort pain_death --character yelling_man --voice "
              << bank << " --out hit." << ext << "\n"
              << "  • generate your own bank from a TTS voice (needs Piper): see SETUP.md\n";
    return 0;
}

// coverage — report how well a bank covers a script. Runs lines through the
// phoneme-backed planner and tallies, per requested unit, whether the bank has
// a syllable match, only phoneme matches, or must fall to a grunt. Helps a user
// see where a bank is thin before shipping.
int cmd_coverage(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    if (!a.has("voice") || (!a.has("script") && !a.has("text"))) {
        std::cerr << "usage: grunt coverage --voice <dir> (--script lines.txt | --text \"...\")\n"
                     "  reports syllable/phoneme/grunt fallback rates + missing units.\n";
        return 2;
    }
    std::string err;
    UnitDatabase db;
    if (!db.load(a.get("voice"), err)) { std::cerr << "voice load failed: " << err << "\n"; return 1; }

    // gather lines
    std::vector<std::string> lines;
    if (a.has("text")) lines.push_back(a.get("text"));
    if (a.has("script")) {
        std::ifstream f(a.get("script"));
        if (!f) { std::cerr << "cannot open script: " << a.get("script") << "\n"; return 1; }
        std::string ln;
        while (std::getline(f, ln)) if (!ln.empty()) lines.push_back(ln);
    }

    TextNormalizer norm;
    SyllablePlanner syl;
    PhonemeMapper mapper;
    { std::string d; for (const char* p : {"data/cmudict.dict","data/sample.dict"}) if (mapper.load_dictionary(resource_path(p),d)) break; }

    int total = 0, syll_hit = 0, phon_hit = 0, grunt_only = 0;
    std::map<std::string,int> missing;   // syllable keys with no syllable unit

    for (const auto& line : lines) {
        NormalizedText nt = norm.normalize(line);
        UnitPlan up = syl.plan_phonemic(nt, mapper);
        for (const auto& ru : up.units) {
            total++;
            if (!db.match_key(ru.key).empty()) { syll_hit++; continue; }
            // any constituent phoneme present?
            bool phon = false;
            for (const auto& f : ru.fallback) if (!f.empty() && !db.match_key(f).empty()) { phon = true; break; }
            if (phon) phon_hit++;
            else { grunt_only++; missing[ru.key]++; }
        }
    }

    if (total == 0) { std::cout << "no units requested\n"; return 0; }
    auto pct = [&](int n){ return 100.0 * n / total; };
    std::cout << "coverage for bank '" << db.voice_id() << "' over "
              << lines.size() << " line(s), " << total << " units:\n"
              << "  word/syllable match : " << syll_hit  << "  (" << (int)pct(syll_hit)  << "%)\n"
              << "  phoneme-level only   : " << phon_hit  << "  (" << (int)pct(phon_hit)  << "%)\n"
              << "  grunt fallback       : " << grunt_only<< "  (" << (int)pct(grunt_only)<< "%)\n";
    if (!missing.empty()) {
        std::cout << "top missing units (add these to improve coverage):\n";
        std::vector<std::pair<std::string,int>> m(missing.begin(), missing.end());
        std::sort(m.begin(), m.end(), [](auto&x,auto&y){return x.second>y.second;});
        for (size_t i = 0; i < m.size() && i < 12; ++i)
            std::cout << "  \"" << m[i].first << "\"  x" << m[i].second << "\n";
    }
    return 0;
}

void usage() {
    std::cout <<
    "grunt " << kGruntVersion << " — type text, get game-ready voice clips\n\n"
    "new here?  run:  grunt quickstart      (hear it in one command, no setup)\n\n"
    "commands:\n"
    "  quickstart  render demo clips from the bundled bank (zero config)\n"
    "  synth       render one line to a clip (OGG/Vorbis default)\n"
    "  batch       bake a folder of named clips + bank.json from a CSV\n"
    "  generate    build a bank's units from a CC0/permissive TTS generator\n"
    "  verify      run the provenance ship gate on a voice bank\n"
    "  coverage    report how well a bank covers a script (syllable/phoneme/grunt)\n"
    "  phonemes    convert text to ARPAbet phonemes (--dict for CMUdict)\n\n"
    "  --version   print version\n"
    "run a command with no args for its usage.\n";
}

} // namespace

int main(int argc, char** argv) {
    voc::set_exe_path(argv[0]);
    if (argc < 2) { usage(); return 2; }
    std::string cmd = argv[1];
    if (cmd == "--version" || cmd == "-V") { std::cout << "grunt " << kGruntVersion << "\n"; return 0; }
    if (cmd == "quickstart") return cmd_quickstart(argc, argv);
    if (cmd == "synth")    return cmd_synth(argc, argv);
    if (cmd == "batch")    return cmd_batch(argc, argv);
    if (cmd == "generate") return cmd_generate(argc, argv);
    if (cmd == "verify")   return cmd_verify(argc, argv);
    if (cmd == "coverage") return cmd_coverage(argc, argv);
    if (cmd == "phonemes") return cmd_phonemes(argc, argv);
    if (cmd == "-h" || cmd == "--help" || cmd == "help") { usage(); return 0; }
    std::cerr << "unknown command: " << cmd << "\n";
    usage();
    return 2;
}
