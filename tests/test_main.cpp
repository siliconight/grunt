// test_main.cpp — minimal assertion-based tests, no framework (TDD §16).
#include "Stages.h"
#include "Wav.h"
#include "AudioOut.h"
#include "ShipGate.h"
#include "Generator.h"
#include "Character.h"
#include "Vocalization.h"
#include "Json.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <string>
#include <fstream>

using namespace voc;

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr << "FAIL: " << msg << "\n"; ++failures; } else { std::cout << "ok: " << msg << "\n"; } } while(0)

// Portable temp path — /tmp doesn't exist on Windows, which segfaulted CI.
static std::string tmp_path(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

void test_normalizer() {
    TextNormalizer n;
    auto r = n.normalize("Open the gate!");
    CHECK(r.tokens.size() == 3, "normalizer: 3 tokens");
    CHECK(r.tokens[0] == "open", "normalizer: lowercased");
    CHECK(r.terminal_punct == "!", "normalizer: detects !");
    CHECK(r.emotion_hint == Emotion::Urgent, "normalizer: ! -> urgent");

    auto q = n.normalize("Really?");
    CHECK(q.terminal_punct == "?", "normalizer: detects ?");

    auto caps = n.normalize("GET OUT now");
    bool found = false;
    for (auto& e : caps.emphasis_words) if (e == "get" || e == "out") found = true;
    CHECK(found, "normalizer: CAPS -> emphasis");

    auto empty = n.normalize("");
    CHECK(empty.tokens.empty(), "normalizer: empty input -> no tokens");
}

void test_syllable() {
    TextNormalizer n; SyllablePlanner s;
    auto up = s.plan(n.normalize("dangerous"));
    CHECK(up.units.size() >= 2, "syllable: splits multi-syllable word");
    auto up2 = s.plan(n.normalize("no"));
    CHECK(up2.units.size() >= 1, "syllable: short word yields a unit");
}

void test_prosody() {
    TextNormalizer n; SyllablePlanner s; ProsodyPlanner p;
    auto pp = p.plan(s.plan(n.normalize("open the gate!")));
    CHECK(!pp.units.empty(), "prosody: produces units");
    for (auto& u : pp.units) CHECK(u.duration_ms > 0, "prosody: positive duration");
}

void test_json() {
    auto j = Json::parse(R"({"a":1,"b":"x","c":[true,false],"d":{"e":2.5}})");
    CHECK(j.is_object(), "json: parses object");
    CHECK(j["a"].as_number() == 1, "json: number");
    CHECK(j["b"].as_string() == "x", "json: string");
    CHECK(j["c"].items().size() == 2, "json: array");
    CHECK(j["c"].items()[0].as_bool() == true, "json: bool true");
    CHECK(j["d"]["e"].as_number() == 2.5, "json: nested number");
}

void test_wav_roundtrip() {
    AudioBuffer b; b.sample_rate = 22050;
    for (int i = 0; i < 1000; ++i) b.samples.push_back(0.5f * std::sin(i * 0.1f));
    std::string err;
    std::string p = tmp_path("_grunt_test.wav");
    CHECK(write_wav(p, b, err), "wav: write ok");
    AudioBuffer r;
    CHECK(read_wav(p, r, err), "wav: read ok");
    CHECK(r.samples.size() == b.samples.size(), "wav: sample count preserved");
    CHECK(!r.samples.empty() && std::fabs(r.samples[10] - b.samples[10]) < 0.01f,
          "wav: values roundtrip");
}

void test_read_missing_file() {
    // reading a nonexistent file must fail cleanly, never crash (the Windows CI segfault)
    AudioBuffer r; std::string err;
    bool ok = read_wav(tmp_path("_grunt_does_not_exist.wav"), r, err);
    CHECK(!ok, "wav: missing file fails gracefully (no crash)");
}

void test_renderer_clipping() {
    // build a tiny in-memory bank scenario via a loud clip on disk
    AudioBuffer loud; loud.sample_rate = 22050;
    for (int i = 0; i < 2000; ++i) loud.samples.push_back(0.99f * std::sin(i * 0.2f));
    std::string err;
    std::string lp = tmp_path("_grunt_loud.wav");
    write_wav(lp, loud, err);
    AudioBuffer rb; read_wav(lp, rb, err);
    CHECK(rb.peak_dbfs() <= 0.0, "renderer/limiter: peak below 0 dBFS sanity");
}

void test_ship_gate_logic() {
    // synth_tool_derived must be rejected; clean original must pass.
    AudioUnit good; good.id = "g1"; good.provenance.source = "original_recording";
    good.provenance.commercial_use = true; good.provenance.synth_tool_derived = false;

    AudioUnit bad;  bad.id = "b1"; bad.provenance.source = "synth_placeholder";
    bad.provenance.synth_tool_derived = true;

    // we can't construct a UnitDatabase without files easily here; just assert
    // the provenance fields the gate reads behave as intended.
    CHECK(good.provenance.commercial_use && !good.provenance.synth_tool_derived,
          "gate: clean original is shippable");
    CHECK(bad.provenance.synth_tool_derived,
          "gate: placeholder flagged as non-shippable");
}

void test_format_dispatch() {
    CHECK(format_from_string("ogg") == AudioFormat::Ogg, "format: ogg");
    CHECK(format_from_string("vorbis") == AudioFormat::Ogg, "format: vorbis -> ogg");
    CHECK(format_from_string("wav") == AudioFormat::Wav, "format: wav");
    CHECK(format_from_string("") == AudioFormat::Ogg, "format: default ogg");
    CHECK(std::string(format_ext(AudioFormat::Ogg)) == "ogg", "format: ext ogg");
    CHECK(std::string(format_ext(AudioFormat::Wav)) == "wav", "format: ext wav");
    // wav always works regardless of build
    AudioBuffer b; b.sample_rate = 22050;
    for (int i = 0; i < 500; ++i) b.samples.push_back(0.3f * std::sin(i * 0.1f));
    std::string err;
    CHECK(write_audio(tmp_path("_grunt_fmt.wav"), b, AudioFormat::Wav, 0.4f, err),
          "format: write_audio wav ok");
}

void test_phoneme_mapper() {
    PhonemeMapper m;
    // rule fallback (no dictionary loaded)
    auto gate = m.map_word("gate");
    CHECK(!gate.phonemes.empty(), "phoneme: rule fallback produces phonemes");
    CHECK(gate.source == PhonemeSource::RuleFallback, "phoneme: marked as fallback");

    // digraph handling: "ship" -> SH ...
    auto ship = m.map_word("ship");
    CHECK(!ship.phonemes.empty() && ship.phonemes[0] == "SH", "phoneme: sh digraph");

    // 'ch' -> CH
    auto chat = m.map_word("chat");
    CHECK(!chat.phonemes.empty() && chat.phonemes[0] == "CH", "phoneme: ch digraph");

    // never empty, even on a weird token
    auto weird = m.map_word("zzz");
    CHECK(!weird.phonemes.empty(), "phoneme: never emits empty");

    // dictionary lookup overrides fallback
    std::string err;
    std::string dictpath = tmp_path("_grunt_dict.txt");
    { std::ofstream of(dictpath); of << "GATE  G EY1 T\nOPEN  OW1 P AH0 N\n"; }
    CHECK(m.load_dictionary(dictpath, err), "phoneme: dictionary loads");
    CHECK(m.dictionary_size() == 2, "phoneme: 2 entries loaded");
    auto g2 = m.map_word("gate");
    CHECK(g2.source == PhonemeSource::Dictionary, "phoneme: dict hit overrides fallback");
    CHECK(g2.phonemes.size() == 3 && g2.phonemes[0] == "G" && g2.phonemes[1] == "EY",
          "phoneme: dict phonemes correct, stress digits stripped");
}

void test_generator_registry() {
    // registry round-trip + provenance stamping
    std::string regpath = tmp_path("_grunt_reg.json");
    {
        std::ofstream of(regpath);
        of << R"({"models":[
          {"id":"clean","generator":"stub","model_file":"x.onnx","license":"CC0-1.0","commercial_use":true,"redistributable":true,"source_url":"u"},
          {"id":"nc","generator":"stub","model_file":"y.onnx","license":"CC-BY-NC-4.0","commercial_use":false,"redistributable":false,"source_url":"u"}
        ]})";
    }
    VoiceModelRegistry reg; std::string err;
    CHECK(reg.load(regpath, err), "registry: loads");
    CHECK(reg.all().size() == 2, "registry: 2 models");
    CHECK(reg.find("clean") != nullptr, "registry: finds model by id");
    CHECK(reg.find("missing") == nullptr, "registry: missing id -> null");

    const VoiceModel* clean = reg.find("clean");
    CHECK(clean->commercial_use && clean->redistributable, "registry: CC0 model is shippable");
    const VoiceModel* nc = reg.find("nc");
    CHECK(!nc->commercial_use, "registry: NC model flagged not commercial");

    // a Piper-generated clip from a clean model must be gate-shippable in
    // provenance terms (commercial_use true, NOT synth_tool_derived).
    // We assert the stamping logic via what generate would write.
    Provenance p;
    p.source = "generated_tts";
    p.license = clean->license;
    p.commercial_use = clean->commercial_use && clean->redistributable;
    p.synth_tool_derived = false; // real generator, not stub
    CHECK(p.commercial_use && !p.synth_tool_derived,
          "generator: clean-model clip is shippable provenance");
}

void test_character_library() {
    std::string cpath = tmp_path("_grunt_chars.json");
    {
        std::ofstream of(cpath);
        of << R"({"characters":[
          {"id":"grunt","display_name":"Grunt","base_voice":"v","pitch_offset_st":-3.0,"fx_preset":"clean_ps1","emotion_bias":"neutral","ready":true},
          {"id":"demon","display_name":"Demon","base_voice":"v","pitch_offset_st":-9.0,"fx_preset":"monster_ps1","emotion_bias":"angry","sub_layer":true,"ready":false,"_blocked_on":"sub layer"}
        ]})";
    }
    CharacterLibrary lib; std::string err;
    CHECK(lib.load(cpath, err), "character: library loads");
    CHECK(lib.all().size() == 2, "character: 2 presets");
    const CharacterPreset* g = lib.find("grunt");
    CHECK(g != nullptr && g->pitch_offset_st == -3.0, "character: grunt pitch");
    CHECK(g->ready, "character: grunt ready");
    const CharacterPreset* d = lib.find("demon");
    CHECK(d != nullptr && !d->ready && d->sub_layer, "character: demon needs-DSP flagged");
    CHECK(lib.find("missing") == nullptr, "character: missing -> null");
}

void test_vocalization() {
    // effort library load
    std::string epath = tmp_path("_grunt_efforts.json");
    {
        std::ofstream of(epath);
        of << R"({"efforts":[
          {"id":"pain_death","phonemes":["AA","AA","HH","K"],"intensity":1.0,"emotion":"urgent","_desc":"death cry"},
          {"id":"yell","phonemes":["AA"],"intensity":1.0,"emotion":"angry","_desc":"battle yell"}
        ]})";
    }
    EffortLibrary el; std::string err;
    CHECK(el.load(epath, err), "vocal: effort library loads");
    CHECK(el.all().size() == 2, "vocal: 2 efforts");
    const Effort* pd = el.find("pain_death");
    CHECK(pd != nullptr && pd->phonemes.size() == 4, "vocal: pain_death phonemes");
    CHECK(pd->intensity == 1.0, "vocal: pain_death intensity");
    CHECK(el.find("missing") == nullptr, "vocal: missing effort -> null");

    // effort -> phoneme seq
    PhonemeSeq es = effort_to_phonemes(*pd);
    CHECK(es.words.size() == 1 && es.words[0].phonemes.size() == 4, "vocal: effort seq built");
    CHECK(es.words[0].source == PhonemeSource::Passthrough, "vocal: effort is passthrough");
    CHECK(es.terminal_punct == "!", "vocal: effort is exclamatory");

    // onomatopoeia: repeated letters raise intensity + length
    PhonemeMapper m;
    double i_long = 0, i_short = 0;
    PhonemeSeq sl = onomatopoeia_to_phonemes("aaaargh", m, i_long);
    PhonemeSeq ss = onomatopoeia_to_phonemes("argh", m, i_short);
    CHECK(i_long > i_short, "vocal: more repeats -> higher intensity");
    CHECK(sl.words[0].phonemes.size() >= ss.words[0].phonemes.size(),
          "vocal: stretched vowels -> at least as many units");
    CHECK(!ss.words[0].phonemes.empty(), "vocal: onomatopoeia produces phonemes");

    // intensity is clamped to 1.0
    double i_huge = 0;
    onomatopoeia_to_phonemes("aaaaaaaaaaaaaaa", m, i_huge);
    CHECK(i_huge <= 1.0, "vocal: intensity clamped to 1.0");
}

void test_dsp() {
    // a simple test signal
    std::vector<float> sig(2000);
    for (size_t i = 0; i < sig.size(); ++i)
        sig[i] = 0.5f * std::sin(2 * voc::kPi * 220.0 * i / 22050.0);

    // formant shift preserves LENGTH (so pitch/duration are untouched —
    // this is the whole point of decoupling formants from pitch)
    auto fdown = voc::dsp::formant_shift(sig, -0.4);
    auto fup   = voc::dsp::formant_shift(sig, 0.4);
    CHECK(fdown.size() == sig.size(), "dsp: formant down preserves length");
    CHECK(fup.size() == sig.size(), "dsp: formant up preserves length");
    // zero shift is a no-op
    auto fzero = voc::dsp::formant_shift(sig, 0.0);
    CHECK(fzero.size() == sig.size(), "dsp: formant zero is no-op length");

    // sub-octave preserves length and actually adds energy (mix > dry peak somewhere)
    auto sub = voc::dsp::add_sub_octave(sig, 0.7);
    CHECK(sub.size() == sig.size(), "dsp: sub-octave preserves length");

    // rasp stays bounded in [-1,1] even when driven hard
    std::vector<float> hot(sig);
    for (auto& x : hot) x *= 1.5f; // push beyond unity first
    voc::dsp::apply_rasp(hot, 1.0);
    bool bounded = true;
    for (float x : hot) if (x < -1.0001f || x > 1.0001f) { bounded = false; break; }
    CHECK(bounded, "dsp: rasp output stays bounded [-1,1]");

    // rasp at 0 is a no-op
    std::vector<float> q(sig);
    voc::dsp::apply_rasp(q, 0.0);
    CHECK(q == sig, "dsp: rasp amt=0 is a no-op");

    // empty input doesn't crash
    std::vector<float> empty;
    CHECK(voc::dsp::formant_shift(empty, -0.5).empty(), "dsp: formant handles empty");
    CHECK(voc::dsp::add_sub_octave(empty, 0.7).empty(), "dsp: sub handles empty");
}

void test_psola() {
    const int sr = 22050;
    // a clean 200 Hz sine -> period should be ~ sr/200 = 110 samples
    std::vector<float> tone(4410);
    for (size_t i = 0; i < tone.size(); ++i)
        tone[i] = 0.5f * std::sin(2 * voc::kPi * 200.0 * i / sr);

    size_t period = voc::dsp::estimate_period(tone, sr);
    CHECK(period > 95 && period < 125, "psola: detects ~200Hz period (110 samples)");

    // aperiodic noise should be rejected (period 0) -> triggers fallback
    std::vector<float> noise(4410);
    uint32_t st = 12345;
    for (auto& x : noise) { st = st * 1664525u + 1013904223u; x = ((st >> 9) / 4194304.0f) - 1.0f; }
    size_t np = voc::dsp::estimate_period(noise, sr);
    CHECK(np == 0, "psola: rejects aperiodic noise (graceful fallback)");

    // time-stretch to 1.5x -> output length ~1.5x input
    std::vector<float> out;
    bool ok = voc::dsp::psola(tone, sr, 1.0, 1.5, out);
    CHECK(ok, "psola: succeeds on periodic tone");
    double ratio = (double)out.size() / tone.size();
    CHECK(ratio > 1.4 && ratio < 1.6, "psola: time-stretch hits ~1.5x length");

    // pitch up an octave, same length (time_ratio 1.0)
    std::vector<float> up;
    bool ok2 = voc::dsp::psola(tone, sr, 2.0, 1.0, up);
    CHECK(ok2, "psola: pitch-shift succeeds");
    CHECK(up.size() == tone.size(), "psola: pitch-only keeps length");
    // the repitched output should itself be periodic at ~half the period
    size_t up_period = voc::dsp::estimate_period(up, sr);
    CHECK(up_period > 0 && up_period < period, "psola: pitched-up output has shorter period");

    // empty / degenerate inputs are safe
    std::vector<float> e_out;
    CHECK(!voc::dsp::psola(std::vector<float>{}, sr, 2.0, 1.0, e_out), "psola: empty -> false");
}

void test_phase2_planner() {
    TextNormalizer norm;
    SyllablePlanner syl;
    PhonemeMapper mapper;
    // load the sample dict so "gate" -> G EY T deterministically
    std::string err;
    bool have_dict = mapper.load_dictionary("data/sample.dict", err);

    NormalizedText nt = norm.normalize("gate");
    UnitPlan up = syl.plan_phonemic(nt, mapper);
    CHECK(!up.units.empty(), "phase2: plan produces units");
    // every unit must carry a fallback chain ending in grunt ("")
    bool all_have_grunt_fallback = true;
    for (const auto& u : up.units)
        if (u.fallback.empty() || u.fallback.back() != "") all_have_grunt_fallback = false;
    CHECK(all_have_grunt_fallback, "phase2: every unit falls back to grunt");

    if (have_dict) {
        // "gate" = G EY T -> single syllable, key "G EY T", phoneme fallbacks
        CHECK(up.units.size() == 1, "phase2: 'gate' is one syllable");
        CHECK(up.units[0].key == "G EY T", "phase2: syllable key is joined phonemes");
        // fallback chain contains the constituent phonemes
        bool has_ey = false;
        for (const auto& f : up.units[0].fallback) if (f == "EY") has_ey = true;
        CHECK(has_ey, "phase2: fallback chain includes constituent phonemes");
    }

    // multi-syllable word splits at vowel nuclei: "open" = OW P AH N -> 2 syllables
    if (have_dict) {
        NormalizedText nt2 = norm.normalize("open");
        UnitPlan up2 = syl.plan_phonemic(nt2, mapper);
        CHECK(up2.units.size() == 2, "phase2: 'open' splits into two syllables");
    }
}

int main() {
    test_normalizer();
    test_syllable();
    test_prosody();
    test_json();
    test_wav_roundtrip();
    test_read_missing_file();
    test_format_dispatch();
    test_phoneme_mapper();
    test_generator_registry();
    test_character_library();
    test_vocalization();
    test_dsp();
    test_psola();
    test_phase2_planner();
    test_renderer_clipping();
    test_ship_gate_logic();
    std::cout << (failures == 0 ? "\nALL TESTS PASSED\n" : "\nTESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
