#include "Engine.h"
#include "ResourcePath.h"
#include "Generator.h"
#include "Stages.h"      // voc::dsp
#include "Prosody.h"
#include "Wav.h"
#include <cctype>
#include <filesystem>
#include <random>

namespace voc {

bool Engine::load_voice(const std::string& voice_dir, std::string& err) {
    UnitDatabase fresh;
    if (!fresh.load(voice_dir, err)) { loaded_ = false; return false; }
    db_ = std::move(fresh);
    loaded_ = true;
    return true;
}

SynthResult Engine::synth(const std::string& text, Emotion emotion,
                          const std::string& fx_preset, uint64_t seed) {
    return synth(text, emotion, fx_preset, seed, Options{});
}

SynthResult Engine::synth(const std::string& text,
                          Emotion emotion,
                          const std::string& fx_preset,
                          uint64_t seed,
                          const Options& opts) {
    SynthResult r;
    if (!loaded_) { r.error = "no voice bank loaded"; return r; }

    TextNormalizer norm;
    SyllablePlanner syl;
    PhonemeMapper mapper;
    ProsodyPlanner pros;
    UnitSelector sel(seed);
    AudioRenderer rend;
    RetroFxChain fx;

    // Load a phoneme dictionary if one is available (improves syllabification);
    // without it the mapper uses rule-based G2P, so this is best-effort.
    {
        std::string derr;
        for (const char* p : {"data/cmudict.dict", "data/sample.dict"}) {
            if (mapper.load_dictionary(resource_path(p), derr)) break;
        }
    }

    NormalizedText nt = norm.normalize(text);
    // Phase 2: phoneme-backed planning supersedes the Phase 0 spelling splitter.
    // Phrase-aware: prefer whole baked phrases from the bank (limited-domain),
    // falling back to word -> syllable -> phoneme -> grunt.
    UnitPlan up = syl.plan_phonemic(nt, mapper, db_);
    if (emotion != Emotion::Neutral) up.emotion = emotion; // explicit override
    ProsodyPlan pp = pros.plan(up);

    // layer character pitch/gain/DSP over every unit
    if (opts.extra_pitch_st != 0.0 || opts.extra_gain_db != 0.0 ||
        opts.formant_shift != 0.0 || opts.sub_layer || opts.rasp != 0.0) {
        for (auto& u : pp.units) {
            u.pitch_offset_st += opts.extra_pitch_st;
            u.gain_db += opts.extra_gain_db;
            u.formant_shift = opts.formant_shift;
            u.sub_layer = opts.sub_layer;
            u.rasp = opts.rasp;
        }
    }

    auto selected = sel.select(pp, db_);

    r.audio = rend.render(selected, db_);
    fx.apply(r.audio, fx_preset);
    r.units = (int)selected.size();
    r.peak_dbfs = r.audio.peak_dbfs();
    r.ok = true;
    return r;
}

SynthResult Engine::synth_vocalization(const PhonemeSeq& seq,
                                       double intensity,
                                       const std::string& fx_preset,
                                       uint64_t seed,
                                       const Options& opts) {
    SynthResult r;
    if (!loaded_) { r.error = "no voice bank loaded"; return r; }

    ProsodyPlanner pros;
    UnitSelector sel(seed);
    AudioRenderer rend;
    RetroFxChain fx;

    // Build a UnitPlan directly from the phoneme sequence: each phoneme becomes
    // a requested unit key (lowercased ARPAbet). The selector matches bank
    // units by key and falls back to grunts, so this works whether or not the
    // bank has dedicated effort units.
    UnitPlan up;
    up.emotion = seq.emotion;
    up.terminal_punct = seq.terminal_punct;
    for (const auto& w : seq.words) {
        for (const auto& ph : w.phonemes) {
            RequestedUnit ru;
            ru.key = ph;
            for (auto& ch : ru.key) ch = (char)std::tolower((unsigned char)ch);
            ru.preferred = UnitType::Phoneme;
            ru.fallback = {""};   // grunt fallback if no phoneme/effort unit
            ru.is_emphasis = w.is_emphasis;
            up.units.push_back(ru);
        }
    }
    if (up.units.empty()) { r.error = "vocalization produced no units"; return r; }

    ProsodyPlan pp = pros.plan(up);

    // intensity (0..1) -> extra gain (up to +6 dB) and longer holds (up to +60%)
    double gain_boost = 6.0 * intensity;
    double dur_scale  = 1.0 + 0.6 * intensity;
    for (auto& u : pp.units) {
        u.gain_db += gain_boost + opts.extra_gain_db;
        u.pitch_offset_st += opts.extra_pitch_st;
        u.duration_ms = (int)(u.duration_ms * dur_scale);
        u.formant_shift = opts.formant_shift;
        u.sub_layer = opts.sub_layer;
        u.rasp = opts.rasp;
    }

    auto selected = sel.select(pp, db_);
    r.audio = rend.render(selected, db_);
    fx.apply(r.audio, fx_preset);
    r.units = (int)selected.size();
    r.peak_dbfs = r.audio.peak_dbfs();
    r.ok = true;
    return r;
}

SynthResult Engine::synth_speech(const std::string& text,
                                 const std::string& model_id,
                                 const std::string& fx_preset,
                                 const Options& opts,
                                 double speed,
                                 const std::string& generator_override) {
    SynthResult r;

    // 1) Resolve the Piper voice model from the registry.
    VoiceModelRegistry reg;
    std::string err;
    if (!reg.load(resource_path("data/voice_models.json"), err)) {
        r.error = "registry: " + err; return r;
    }
    const VoiceModel* model = reg.find(model_id);
    if (!model) { r.error = "voice model '" + model_id + "' not in registry"; return r; }

    // Is the model actually downloaded? The registry only says it's *known*; the
    // .onnx has to be on disk next to the binary (fetch-voice puts it there).
    // Checking here turns piper's cryptic "Unable to find voice: <name>"
    // traceback into a clear, actionable message naming the fetch command.
    if (generator_override.empty() && model->generator == "piper") {
        std::error_code mec;
        if (!std::filesystem::exists(resource_path(model->model_file), mec)) {
            r.error = "voice '" + model_id + "' isn't downloaded yet. Get it with: "
                      "grunt fetch-voice --model " + model_id;
            r.missing_model = model_id;   // lets batch fall back to a present voice
            return r;
        }
    }

    // 2) Synthesize the WHOLE line with Piper into a temp wav — one clean,
    //    intelligible utterance, any words. (Same generator the bake uses; this
    //    is authoring-time synthesis, the game still ships only the baked OGG.)
    Generator* gen = make_generator(generator_override.empty() ? model->generator
                                                               : generator_override);
    if (!gen) { r.error = "no generator for model"; return r; }

    std::error_code ec;
    auto tmp = std::filesystem::temp_directory_path(ec);
    std::string tmp_dir = (tmp / ("grunt_speech_" +
        std::to_string(std::random_device{}()))).string();
    std::filesystem::create_directories(tmp_dir, ec);

    // Punctuation -> inflection: when punchy is on, rewrite the line for stronger
    // prosody and derive a per-line sentence silence; otherwise both are no-ops.
    std::string speak_text = punchify_text(text, opts.punchy);
    double sil = sentence_silence_for(speak_text, opts.punchy);
    GeneratedClip clip = gen->generate("line", speak_text, *model, tmp_dir, sil);
    delete gen;
    if (!clip.ok) {
        std::filesystem::remove_all(tmp_dir, ec);
        r.error = "speech synthesis failed: " + clip.error; return r;
    }

    AudioBuffer buf;
    if (!read_wav(clip.wav_path, buf, err) || buf.samples.empty()) {
        std::filesystem::remove_all(tmp_dir, ec);
        r.error = "could not read synthesized audio: " + err; return r;
    }
    std::filesystem::remove_all(tmp_dir, ec);  // clip is in memory now

    const int sr = buf.sample_rate;
    std::vector<float>& s = buf.samples;

    // 3) Style the WHOLE utterance at its NATURAL length. This is the core of
    //    the new design: no unit stitching, no duration-fit crushing the word
    //    (that was what made speech fast + high-pitched). Order: formant ->
    //    pitch (length-preserving) -> sub -> rasp -> gain.

    // formant shift (spectral envelope; length/pitch preserved)
    if (std::fabs(opts.formant_shift) > 1e-3)
        s = dsp::formant_shift(s, opts.formant_shift);

    // pitch shift WITHOUT changing tempo: resample (moves pitch+length) then
    // PSOLA time-stretch back to the natural length (× 1/speed). pitch_ratio<1
    // => lower/gruffer. speed<1 => slower (longer); speed>1 => faster.
    double pitch_ratio = std::pow(2.0, opts.extra_pitch_st / 12.0);
    bool need_pitch = std::fabs(opts.extra_pitch_st) > 1e-3;
    bool need_speed = std::fabs(speed - 1.0) > 1e-3;
    if (need_pitch || need_speed) {
        size_t target_n = (size_t)((double)s.size() / (speed > 0 ? speed : 1.0));
        std::vector<float> out;
        if (dsp::psola(s, sr, need_pitch ? pitch_ratio : 1.0,
                       target_n > 0 ? (double)target_n / (double)s.size() : 1.0,
                       out) && !out.empty()) {
            s = std::move(out);
        } else {
            // fallback: resample for pitch, then linear-fit back to target length
            if (need_pitch) {
                std::vector<float> rs((size_t)std::max<double>(1.0, s.size() / pitch_ratio));
                for (size_t i = 0; i < rs.size(); ++i) {
                    double src = i * pitch_ratio; size_t i0 = (size_t)src;
                    double f = src - i0;
                    float a = s[std::min(i0, s.size()-1)];
                    float b = s[std::min(i0+1, s.size()-1)];
                    rs[i] = (float)(a + (b-a)*f);
                }
                s = std::move(rs);
            }
            if (target_n > 0 && target_n != s.size()) {
                std::vector<float> ft(target_n);
                double ratio = (double)s.size() / (double)target_n;
                for (size_t i = 0; i < target_n; ++i) {
                    double src = i * ratio; size_t i0 = (size_t)src;
                    double f = src - i0;
                    float a = s[std::min(i0, s.size()-1)];
                    float b = s[std::min(i0+1, s.size()-1)];
                    ft[i] = (float)(a + (b-a)*f);
                }
                s = std::move(ft);
            }
        }
    }

    if (opts.sub_layer) s = dsp::add_sub_octave(s, 0.7);
    if (opts.rasp > 1e-3) dsp::apply_rasp(s, opts.rasp);
    if (std::fabs(opts.extra_gain_db) > 1e-3) {
        double g = std::pow(10.0, opts.extra_gain_db / 20.0);
        for (auto& x : s) x = (float)(x * g);
    }

    // 4) PS1 retro FX chain over the whole styled line.
    RetroFxChain fx;
    fx.apply(buf, fx_preset);

    r.audio = std::move(buf);
    r.units = 1;                       // one whole utterance
    r.peak_dbfs = r.audio.peak_dbfs();
    r.ok = true;
    return r;
}

} // namespace voc
