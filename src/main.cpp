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
#include "BankGen.h"

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
    // --effort reads bank units (--voice required). --text and --onomatopoeia
    // speak through Piper and style the result (no bank); --text with --voice
    // instead stitches from that bank (old concatenative path).
    bool text_uses_bank = a.has("text") && a.has("voice");
    bool needs_bank = a.has("effort") || text_uses_bank;
    if (!has_input || !a.has("out") ||
        (a.has("effort") && !a.has("voice"))) {
        std::cerr << "usage: grunt synth (--text \"...\" | --effort <id> | --onomatopoeia \"aaargh\")"
                     " --out <file>"
                     " [--character <name>] [--model <id>] [--speed 1.0]"
                     " [--emotion neutral|urgent|angry]"
                     " [--style clean_ps1] [--format ogg|wav] [--quality 0.4] [--seed N]\n"
                     "  --text/--onomatopoeia speak via Piper, then style the result (no bank).\n"
                     "          --model picks the voice (default piper-en_US-ljspeech or the\n"
                     "          character's base voice); --speed <1 slower, >1 faster.\n"
                     "  --effort renders from a bank (--voice <dir> required).\n"
                     "  --character applies a preset from data/characters.json (pitch/formant/rasp/FX).\n"
                     "  default format: ogg (Vorbis). out extension is set to match the format.\n";
        return 2;
    }
    std::string err;

    Emotion emo = a.has("emotion") ? emotion_from_string(a.get("emotion")) : Emotion::Neutral;
    std::string fx = a.get("style", "clean_ps1");
    uint64_t seed = a.has("seed") ? std::stoull(a.get("seed")) : 0x9E3779B97F4A7C15ULL;
    Engine::Options opts;
    std::string char_base_voice;  // for the Piper speech path's model choice

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
        char_base_voice     = cp->base_voice;
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
    if (needs_bank) {
        if (!engine.load_voice(a.get("voice"), err)) { std::cerr << "voice load failed: " << err << "\n"; return 1; }
    }

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
        // Post-pivot: speak the sound through Piper (no bank), styled by the
        // character FX — same path as --text. Repeated letters slow delivery so
        // "aaaargh" drawls longer than "argh".
        std::string ono = a.get("onomatopoeia");
        int max_run = 1, run = 1;
        for (size_t i = 1; i < ono.size(); ++i) {
            if (std::tolower((unsigned char)ono[i]) == std::tolower((unsigned char)ono[i-1])) {
                if (++run > max_run) max_run = run;
            } else run = 1;
        }
        double speed = 1.0 - 0.08 * (max_run - 1);
        if (speed < 0.6) speed = 0.6;
        if (a.has("speed")) speed = std::stod(a.get("speed"));  // explicit override wins
        std::string model_id = a.get("model",
            (a.has("character") && !char_base_voice.empty()) ? char_base_voice
                                                             : "piper-en_US-ljspeech");
        res = engine.synth_speech(ono, model_id, fx, opts, speed, a.get("generator", ""));
    } else if (text_uses_bank) {
        // --text WITH a --voice bank: stitch from that bank (the concatenative
        // path — e.g. a grunt-only bank turning words into grunts).
        res = engine.synth(a.get("text"), emo, fx, seed, opts);
    } else {
        // --text with NO bank: speak the whole line via Piper and style it (the
        // primary path — says any words, then makes them sound PS1). Voice:
        // explicit --model, else the character's base voice, else LJ.
        std::string model_id = a.get("model",
            (a.has("character") && !char_base_voice.empty()) ? char_base_voice
                                                             : "piper-en_US-ljspeech");
        double speed = a.has("speed") ? std::stod(a.get("speed")) : 1.0;
        res = engine.synth_speech(a.get("text"), model_id, fx, opts, speed,
                                  a.get("generator", ""));
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
    if (!a.has("csv") || !a.has("out-dir")) {
        std::cerr << "usage: grunt batch --csv <lines.csv> --out-dir <dir>"
                     " [--character <name>] [--model <id>] [--speed 1.0]"
                     " [--style clean_ps1] [--format ogg|wav] [--quality 0.4] [--seed N]\n"
                     "  Speaks each line via Piper and styles it (no bank needed).\n"
                     "  CSV columns: key,text[,character]  (a per-line character\n"
                     "  overrides --character; header row 'key,text' is skipped).\n"
                     "  default format: ogg (Vorbis).\n";
        return 2;
    }
    Engine engine; std::string err;

    // Load the character library once (presets supply voice + pitch/formant/rasp/FX).
    CharacterLibrary clib;
    bool have_clib = clib.load(a.get("characters", resource_path("data/characters.json")), err);

    std::ifstream csv(a.get("csv"));
    if (!csv) { std::cerr << "cannot open csv: " << a.get("csv") << "\n"; return 1; }

    std::string default_fx = a.get("style", "clean_ps1");
    std::string default_char = a.get("character", "");
    std::string default_model = a.get("model", "");
    double speed = a.has("speed") ? std::stod(a.get("speed")) : 1.0;
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
    manifest << "{\n  \"bank_id\": \"" << json_escape(
                 default_char.empty() ? std::string("vo") : default_char) << "_vo\",\n"
             << "  \"schema_version\": 1,\n  \"clips\": [\n";

    // Resolve a character preset name -> Options + fx + model. Returns false if
    // the named character isn't found (so the line can be skipped with a note).
    auto resolve_char = [&](const std::string& cname, Engine::Options& opts,
                            std::string& fx, std::string& model) -> bool {
        opts = Engine::Options{};
        fx = default_fx;
        model = default_model;
        if (cname.empty()) {
            if (model.empty()) model = "piper-en_US-ljspeech";
            return true;
        }
        if (!have_clib) return false;
        const CharacterPreset* cp = clib.find(cname);
        if (!cp) return false;
        fx = cp->fx_preset;
        opts.extra_pitch_st = cp->pitch_offset_st;
        opts.extra_gain_db  = cp->gain_db;
        opts.formant_shift  = cp->formant_shift;
        opts.sub_layer      = cp->sub_layer;
        opts.rasp           = cp->rasp ? 0.6 : 0.0;
        if (model.empty()) model = cp->base_voice.empty() ? "piper-en_US-ljspeech" : cp->base_voice;
        return true;
    };

    std::string line; bool first = true; int n = 0; bool header_skipped = false;
    while (std::getline(csv, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        { size_t q0 = line.find_first_not_of(" \t");
          if (q0 == std::string::npos || line[q0] == '#') continue; }  // blank/comment
        std::vector<std::string> f; std::stringstream ss(line); std::string cell;
        while (std::getline(ss, cell, ',')) f.push_back(cell);
        if (f.size() < 2) continue;
        if (!header_skipped && (f[0] == "key" || f[0] == "name")) { header_skipped = true; continue; }
        header_skipped = true;

        std::string name = f[0], text = f[1];
        // trim leading spaces on text (CSV " holy shit" -> "holy shit")
        size_t p0 = text.find_first_not_of(' ');
        if (p0 != std::string::npos) text = text.substr(p0);
        std::string cname = (f.size() > 2 && !f[2].empty()) ? f[2] : default_char;
        // strip spaces around per-line character
        { size_t s0 = cname.find_first_not_of(' '); size_t s1 = cname.find_last_not_of(' ');
          if (s0 != std::string::npos) cname = cname.substr(s0, s1 - s0 + 1); }

        Engine::Options opts; std::string fx, model;
        if (!resolve_char(cname, opts, fx, model)) {
            std::cerr << "  skip " << name << ": character '" << cname << "' not found\n";
            continue;
        }

        SynthResult res = engine.synth_speech(text, model, fx, opts, speed,
                                              a.get("generator", ""));
        if (!res.ok && !res.missing_model.empty() && model != "piper-en_US-ljspeech") {
            // The character's voice isn't downloaded; fall back to the default
            // LJ voice so the bake still produces clips. Note it once per line.
            std::cerr << "  note " << name << ": voice '" << res.missing_model
                      << "' not downloaded, using piper-en_US-ljspeech (fetch it with: "
                      << "grunt fetch-voice --model " << res.missing_model << ")\n";
            res = engine.synth_speech(text, "piper-en_US-ljspeech", fx, opts, speed,
                                      a.get("generator", ""));
        }
        if (!res.ok) { std::cerr << "  skip " << name << ": " << res.error << "\n"; continue; }

        AudioBuffer& buf = res.audio;
        std::string rel = name + "." + ext;
        std::string path = out_dir + "/" + rel;
        if (!write_audio(path, buf, fmt, q, err)) { std::cerr << "  write fail " << name << ": " << err << "\n"; continue; }

        if (!first) manifest << ",\n";
        first = false;
        int dur_ms = (int)(buf.samples.size() * 1000.0 / buf.sample_rate);
        manifest << "    { \"name\": \"" << json_escape(name) << "\""
                 << ", \"file\": \"" << json_escape(rel) << "\""
                 << ", \"text\": \"" << json_escape(text) << "\""
                 << ", \"character\": \"" << json_escape(cname) << "\""
                 << ", \"fx_preset\": \"" << json_escape(fx) << "\""
                 << ", \"duration_ms\": " << dur_ms
                 << ", \"sample_rate\": " << buf.sample_rate
                 << ", \"peak_dbfs\": " << res.peak_dbfs << " }";
        std::cout << "  baked " << name << "  (" << cname << ")\n";
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
                     "  [--unit-type word|syllable]  (default word)\n"
                     "  units.csv rows: key,text  (e.g.  alert,hey)\n"
                     "  word units are keyed by the spoken text; syllable units by the key\n"
                     "  (use ARPAbet keys like \"G EY T\" so the planner's syllable tier matches).\n"
                     "  --model must be a license-cleared id from the registry.\n";
        return 2;
    }
    std::string unit_type = a.get("unit-type", "word");
    if (unit_type != "word" && unit_type != "syllable") {
        std::cerr << "--unit-type must be 'word' or 'syllable'\n"; return 2;
    }

    GenerateOptions opt;
    opt.units_csv     = a.get("units");
    opt.voice_dir     = a.get("voice");
    opt.model_id      = a.get("model");
    opt.registry_path = a.get("registry", resource_path("data/voice_models.json"));
    opt.generator     = a.get("generator", "");   // empty = model default
    opt.unit_type     = unit_type;

    GenerateResult res = generate_bank(opt, [](const std::string& m){
        std::cerr << "  " << m << "\n";
    });
    if (!res.ok) { std::cerr << "generate failed: " << res.error << "\n"; return 1; }
    std::cout << res.message << "\n";
    return 0;
}

// grunt version — bump with each tagged release.
static const char* kGruntVersion = "0.22.11";

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

// fetch-voice — download a registered voice model's files to where grunt looks
// for them, killing the manual "find the .onnx on a website" step. Only models
// with a verified direct download_url are fetched; others print manual steps.
int cmd_fetch_voice(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    std::string reg_path = a.get("registry", resource_path("data/voice_models.json"));
    VoiceModelRegistry reg; std::string err;
    if (!reg.load(reg_path, err)) { std::cerr << "registry: " << err << "\n"; return 1; }

    std::string id = a.get("model");
    if (id.empty()) {
        std::cout << "usage: grunt fetch-voice --model <id>\n\nregistered voices:\n";
        for (const auto& m : reg.all())
            std::cout << "  " << m.id << "  ("
                      << (m.download_url.empty() ? "manual download" : "auto-downloadable")
                      << ")\n";
        return 2;
    }
    const VoiceModel* m = reg.find(id);
    if (!m) { std::cerr << "model '" << id << "' not in registry\n"; return 1; }

    // destination: next to the binary, where generate/doctor resolve model_file
    std::string dest = resource_path(m->model_file);

    auto outcome = voc::fetch_voice_model(*m, dest,
        [](const std::string& line){ std::cout << "  " << line << "\n"; });
    if (!outcome.ok) {
        std::cerr << "[X] " << outcome.err << "\n";
        return 1;
    }
    if (outcome.already_present) return 0;
    std::cout << "[ok] fetched: " << dest << "\n"
              << "now: grunt generate --units examples/barks.csv --voice voices/my_guards --model "
              << id << "\n";
    return 0;
}

// doctor — verify the environment for the generate path before you invest time
// in it. Each check reports OK or a precise fix. The goal: turn "install piper,
// download a model, hope, get a cryptic error" into a checklist that tells you
// exactly what's missing. Exit code is the number of failed checks (0 = ready).
int cmd_doctor(int argc, char** argv) {
    Args a = parse_args(argc, argv, 2);
    bool live = a.has("live");   // also attempt a real one-word generation
    int fails = 0;
    auto ok   = [](const std::string& m){ std::cout << "  [ok]   " << m << "\n"; };
    auto bad  = [&](const std::string& m, const std::string& fix){
        std::cout << "  [MISS] " << m << "\n         fix: " << fix << "\n"; fails++; };

    std::cout << "grunt doctor — checking the generate path\n\n";

    // 1) the registry
    std::string reg_path = a.get("registry", resource_path("data/voice_models.json"));
    VoiceModelRegistry reg; std::string err;
    bool have_reg = reg.load(reg_path, err);
    if (have_reg) ok("voice model registry found (" + std::to_string(reg.all().size()) + " models): " + reg_path);
    else bad("voice model registry not found/parseable: " + err,
             "run grunt from the repo/package root, or pass --registry <path>");

    // 2) piper available — auto-detected (piper / python -m piper / py -m piper).
    std::cout << "checking for piper (speech engine)...\n";
    std::string piper_cmd = detect_piper_cmd();
    bool have_piper = !piper_cmd.empty();
    if (have_piper) {
        ok(std::string("piper available via: ") + piper_cmd);
#if defined(_WIN32)
        _putenv_s("GRUNT_PIPER_CMD", piper_cmd.c_str());
#else
        setenv("GRUNT_PIPER_CMD", piper_cmd.c_str(), 1);
#endif
    } else {
        bad("piper not found",
            "install the modern engine: pip install piper-tts  (needs Python). "
            "grunt auto-detects piper / python -m piper; no env var needed. "
            "You can still use --generator stub for pipeline testing.");
    }

    // 3) at least one registered model's file is present
    if (have_reg) {
        int present = 0;
        for (const auto& m : reg.all()) {
            if (m.generator != "piper") continue;
            std::error_code ec;
            // look next to the registry, in the model dir, and as given
            bool found = std::filesystem::exists(m.model_file, ec) ||
                         std::filesystem::exists(resource_path("voices/models/" + m.model_file), ec) ||
                         std::filesystem::exists(resource_path(m.model_file), ec);
            if (found) { ok("model file present: " + m.model_file + "  (" + m.id + ")"); present++; }
            else std::cout << "  [ -- ] model not downloaded: " << m.model_file
                           << "  (" << m.id << ", " << m.license << ")\n";
        }
        if (present == 0)
            bad("no registered voice-model files are present on disk",
                "download one (see SETUP.md step 2): e.g. norman.onnx + norman.onnx.json "
                "from brycebeattie.com/files/tts, placed where grunt can find it "
                "(next to the binary, or pass --model with a path).");
    }

    // 4) a writable place to put a bank
    {
        std::error_code ec;
        auto tmp = std::filesystem::temp_directory_path(ec) / "grunt_doctor_probe";
        std::filesystem::create_directories(tmp, ec);
        if (!ec) { ok("can create voice-bank directories"); std::filesystem::remove_all(tmp, ec); }
        else bad("cannot create directories for a voice bank", "check filesystem permissions");
    }

    // 5) live generation (optional, needs piper + a model)
    if (live) {
        std::cout << "\nlive check: generating one test word...\n";
        if (!have_reg) { bad("cannot run live check without a registry", "see above"); }
        else {
            // pick the first piper model whose file is present
            const VoiceModel* use = nullptr;
            for (const auto& m : reg.all()) {
                std::error_code ec;
                if (m.generator == "piper" &&
                    (std::filesystem::exists(m.model_file, ec) ||
                     std::filesystem::exists(resource_path(m.model_file), ec)))
                    { use = &m; break; }
            }
            if (!use || !have_piper)
                bad("live check skipped (need piper + a present model)",
                    "complete steps 2–3 above, then re-run: grunt doctor --live");
            else {
                Generator* g = make_generator("piper");
                std::error_code ec;
                auto dir = (std::filesystem::temp_directory_path(ec) / "grunt_doctor_gen").string();
                std::filesystem::create_directories(dir, ec);
                GeneratedClip clip = g->generate("test", "hello", *use, dir);
                if (clip.ok && clip.provenance.commercial_use && !clip.provenance.synth_tool_derived)
                    ok("live generation succeeded and passed the ship gate");
                else if (clip.ok)
                    bad("generated a clip, but it would NOT pass the ship gate",
                        "check the model's license flags in the registry");
                else
                    bad("live generation failed: " + clip.error,
                        "verify piper runs and the model file path is correct");
                delete g;
                std::filesystem::remove_all(dir, ec);
            }
        }
    }

    std::cout << "\n";
    if (fails == 0) {
        std::cout << "ready — the generate path is good to go. Make your first bank:\n"
                  << "  grunt generate --units examples/barks.csv --voice voices/my_guards --model piper-en_US-norman\n"
                  << "  grunt synth --text \"intruder\" --character orc --voice voices/my_guards --out test.ogg\n";
    } else {
        std::cout << fails << " thing(s) to fix above. Re-run `grunt doctor` after each. "
                     "You can always use the zero-setup path now: grunt quickstart\n";
    }
    return fails;
}

void usage() {
    std::cout <<
    "grunt " << kGruntVersion << " — type text, get game-ready voice clips\n\n"
    "new here?  run:  grunt quickstart      (hear it in one command, no setup)\n\n"
    "commands:\n"
    "  quickstart  render demo clips from the bundled bank (zero config)\n"
    "  doctor      check the environment for the generate path (--live to test)\n"
    "  fetch-voice download a registered voice model (--model <id>)\n"
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
    if (cmd == "doctor")     return cmd_doctor(argc, argv);
    if (cmd == "fetch-voice") return cmd_fetch_voice(argc, argv);
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
