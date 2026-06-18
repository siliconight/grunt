// grunt_gui — a minimal desktop front end for grunt.
//
// Type a line, pick voice/emotion/style, hit Play to hear it (no file written),
// then Export to save an OGG/Vorbis (or WAV) clip you drop straight into a
// Godot project. Same Engine as the CLI — one synthesis path, no drift.
//
// Dependencies (vendored, see CMakeLists + GUI build notes in README):
//   - Dear ImGui (docking or master) with the GLFW + OpenGL3 backends
//   - miniaudio (single header) for real-time preview playback
//   - GLFW for the window/context
//
// This file deliberately keeps all grunt-specific logic in Engine; the GUI is
// just glue.

#include "Engine.h"
#include "AudioOut.h"
#include "Types.h"
#include "ResourcePath.h"
#include "Character.h"
#include "Vocalization.h"
#include "BankGen.h"
#include "Generator.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cstdlib>

using namespace voc;

// ---------------------------------------------------------------------------
// Playback: a tiny miniaudio device that streams from a float buffer we own.
// Replacing the buffer (under a lock) is how "Play" works — no temp files.
// ---------------------------------------------------------------------------
struct Player {
    ma_device device{};
    std::mutex mtx;
    std::vector<float> buffer;   // mono samples to play
    size_t cursor = 0;
    std::atomic<bool> playing{false};
    int sample_rate = 22050;

    static void data_cb(ma_device* dev, void* out, const void* /*in*/, ma_uint32 frames) {
        auto* self = static_cast<Player*>(dev->pUserData);
        float* o = static_cast<float*>(out);
        std::lock_guard<std::mutex> lk(self->mtx);
        for (ma_uint32 i = 0; i < frames; ++i) {
            if (self->playing && self->cursor < self->buffer.size())
                o[i] = self->buffer[self->cursor++];
            else {
                o[i] = 0.0f;
                if (self->cursor >= self->buffer.size()) self->playing = false;
            }
        }
    }

    bool init(int sr) {
        sample_rate = sr;
        ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
        cfg.playback.format   = ma_format_f32;
        cfg.playback.channels = 1;
        cfg.sampleRate        = (ma_uint32)sr;
        cfg.dataCallback      = data_cb;
        cfg.pUserData         = this;
        if (ma_device_init(nullptr, &cfg, &device) != MA_SUCCESS) return false;
        return ma_device_start(&device) == MA_SUCCESS;
    }

    void play(const std::vector<float>& samples) {
        std::lock_guard<std::mutex> lk(mtx);
        buffer = samples;
        cursor = 0;
        playing = true;
    }

    void stop() { playing = false; }

    void shutdown() { ma_device_uninit(&device); }
};

// Scan a voices/ root for subdirectories that look like banks (have voice.json),
// returning their names. Used to populate the bank dropdown.
static std::vector<std::string> discover_banks() {
    std::vector<std::string> banks;
    std::error_code ec;
    std::string root = voc::resource_path("voices");
    if (!std::filesystem::is_directory(root, ec)) return banks;
    for (auto& e : std::filesystem::directory_iterator(root, ec)) {
        if (!e.is_directory(ec)) continue;
        if (std::filesystem::exists(e.path() / "voice.json", ec))
            banks.push_back(e.path().filename().string());
    }
    std::sort(banks.begin(), banks.end());
    return banks;
}

// Whether a bank has whole-word units (the reliable intelligibility signal).
// Grunt-only and demo banks lack these, so the UI can flag them honestly.
static bool bank_has_words(const std::string& bank_name) {
    std::error_code ec;
    std::string root = voc::resource_path("voices");
    auto units = std::filesystem::path(root) / bank_name / "metadata" / "units.json";
    std::ifstream f(units);
    if (!f) return false;
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return s.find("\"type\": \"word\"") != std::string::npos
        || s.find("\"type\":\"word\"") != std::string::npos;
}

// Find a usable piper: a `piper` command, the modern Python package
// (`python -m piper`), or a bundled binary next to the exe. Returns the command
// string to invoke (sets nothing; caller exports GRUNT_PIPER_CMD), or "".
static std::string find_piper() {
    std::string cmd = voc::detect_piper_cmd();
    if (!cmd.empty()) return cmd;

    // bundled binary next to the exe (legacy standalone piper)?
    std::error_code ec;
    std::string base = voc::resource_path("piper");
    const char* names[] = {
#if defined(_WIN32)
        "piper\\piper.exe", "piper.exe",
#else
        "piper/piper", "piper",
#endif
    };
    for (const char* n : names) {
        auto p = std::filesystem::path(base) / n;
        if (std::filesystem::exists(p, ec)) return p.string();
    }
    return "";
}

int main(int argc, char** argv) {
    voc::set_exe_path(argv[0]);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(560, 420, "grunt", nullptr, nullptr);
    if (!win) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    // window/taskbar icon (optional — silently skipped if the file isn't found).
    // Resolve exe-relative (not CWD) so it loads no matter where the GUI is
    // launched from, the same way model/registry paths are resolved.
    {
        int iw, ih, ic;
        std::string icon_path = voc::resource_path("assets/grunt_icon64.png");
        unsigned char* px = stbi_load(icon_path.c_str(), &iw, &ih, &ic, 4);
        if (px) {
            GLFWimage icon{ iw, ih, px };
            glfwSetWindowIcon(win, 1, &icon);
            stbi_image_free(px);
        }
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    Engine engine;
    Player player;
    std::string status = "Pick a character, Load, then Play.";
    char voice_dir[256];
    std::snprintf(voice_dir, sizeof(voice_dir), "%s",
                  voc::resource_path("voices/heavy_brother").c_str());
    char text[256] = "Open the gate!";
    int emotion_idx = 0;   // 0 neutral, 1 urgent, 2 angry
    int fx_idx = 0;
    int seed = 42;
    bool lock_seed = true; // deterministic by default
    char export_path[256] = "line.ogg";
    int format_idx = 0;    // 0 ogg, 1 wav
    bool show_advanced = false;

    const char* emotions[] = { "neutral", "urgent", "angry" };
    const char* fxs[] = { "clean_ps1", "radio_ps1", "monster_ps1", "robot_ps1", "muffled_mask" };
    const char* formats[] = { "ogg (Vorbis)", "wav" };

    // Character presets — the primary control. Loaded from data/characters.json
    // (resolved relative to the exe). Selecting one applies its recipe.
    CharacterLibrary char_lib;
    std::vector<std::string> char_ids;       // ids for lookup
    std::vector<std::string> char_labels;    // display names for the combo
    std::vector<const char*> char_label_ptrs;
    {
        std::string cerr;
        if (char_lib.load(voc::resource_path("data/characters.json"), cerr)) {
            for (const auto& c : char_lib.all()) {
                char_ids.push_back(c.id);
                char_labels.push_back(c.display_name.empty() ? c.id : c.display_name);
            }
        }
        if (char_ids.empty()) { char_ids.push_back(""); char_labels.push_back("(none — raw bank)"); }
        for (auto& s : char_labels) char_label_ptrs.push_back(s.c_str());
    }
    int character_idx = 0;

    // Effort vocalizations — loaded from data/efforts.json (same set the CLI's
    // --effort exposes). Populates the Effort-mode dropdown.
    EffortLibrary effort_lib;
    std::vector<std::string> effort_ids;
    std::vector<std::string> effort_labels;
    std::vector<const char*> effort_label_ptrs;
    {
        std::string eerr;
        if (effort_lib.load(voc::resource_path("data/efforts.json"), eerr)) {
            for (const auto& e : effort_lib.all()) {
                effort_ids.push_back(e.id);
                effort_labels.push_back(e.desc.empty() ? e.id : (e.id + " — " + e.desc));
            }
        }
        if (effort_ids.empty()) { effort_ids.push_back(""); effort_labels.push_back("(no efforts.json found)"); }
        for (auto& s : effort_labels) effort_label_ptrs.push_back(s.c_str());
    }
    int effort_idx = 0;

    // Input mode: what kind of utterance to render. Character + seed apply to
    // all three, mirroring how the CLI composes --character with
    // --text / --effort / --onomatopoeia.
    //   0 = Line (spoken text)   1 = Effort (named grunt)   2 = Onomatopoeia
    const char* input_modes[] = { "Line", "Effort", "Onomatopoeia" };
    int input_mode = 0;
    char onomatopoeia[128] = "aaargh";

    // Bank dropdown — discovered word/grunt banks under voices/. Refreshed after
    // a Generate. The selected bank is what Load opens.
    std::vector<std::string> banks = discover_banks();
    std::vector<std::string> bank_labels;
    std::vector<const char*> bank_label_ptrs;
    int bank_idx = 0;
    auto refresh_banks = [&]() {
        banks = discover_banks();
        bank_labels.clear(); bank_label_ptrs.clear();
        for (auto& b : banks)
            bank_labels.push_back(b + (bank_has_words(b) ? "  (words)" : "  (grunts only)"));
        for (auto& s : bank_labels) bank_label_ptrs.push_back(s.c_str());
        if (bank_idx >= (int)banks.size()) bank_idx = 0;
    };
    refresh_banks();
    // default selection: prefer a bank named "starter" (ships speaking real
    // words), else the first word-capable bank, else whatever's first.
    for (size_t i = 0; i < banks.size(); ++i)
        if (banks[i] == "starter") { bank_idx = (int)i; break; }
    if (bank_idx == 0)
        for (size_t i = 0; i < banks.size(); ++i)
            if (bank_has_words(banks[i])) { bank_idx = (int)i; break; }

    // Generate-voices state
    std::string piper_cmd = find_piper();
    char gen_bank_name[128] = "my_guards";
    int gen_model_idx = 0;
    const char* gen_models[] = { "piper-en_US-ljspeech", "piper-en_US-norman" };
    std::string gen_status;

    bool player_ready = false;
    SynthResult last; // cached last render for export

    auto do_synth = [&]() -> bool {
        // Line mode speaks the whole line via Piper — no bank needed. Only the
        // bank-based modes (Effort / Onomatopoeia) require a loaded voice bank.
        if (input_mode != 0 && !engine.voice_loaded()) {
            status = "No voice bank loaded — click Load."; return false;
        }
        uint64_t s = lock_seed ? (uint64_t)seed
                               : (uint64_t)glfwGetTime() * 1000003ULL + 1;

        // Apply the selected character preset (if any) as the CLI does.
        Engine::Options opts;
        Emotion emo = (Emotion)emotion_idx;
        std::string fx = fxs[fx_idx];
        const std::string& cid = char_ids[character_idx];
        std::string speech_model = "piper-en_US-ljspeech";  // default voice for speech
        if (!cid.empty()) {
            if (const CharacterPreset* cp = char_lib.find(cid)) {
                fx  = cp->fx_preset;
                emo = emotion_from_string(cp->emotion_bias);
                opts.extra_pitch_st = cp->pitch_offset_st;
                opts.extra_gain_db  = cp->gain_db;
                opts.formant_shift  = cp->formant_shift;
                opts.sub_layer      = cp->sub_layer;
                opts.rasp           = cp->rasp ? 0.6 : 0.0;
                if (!cp->base_voice.empty()) speech_model = cp->base_voice;
            }
        }
        // Render according to the selected input mode. Character recipe (above)
        // applies to all three; emotion may be overridden per-mode as the CLI does.
        if (input_mode == 1) {                       // Effort
            const std::string& eid = effort_ids[effort_idx];
            const Effort* ef = eid.empty() ? nullptr : effort_lib.find(eid);
            if (!ef) { status = "No effort selected (need data/efforts.json)."; return false; }
            PhonemeSeq seq = effort_to_phonemes(*ef);
            if (cid.empty()) emo = ef->emotion;      // character emotion_bias wins if a character is set
            seq.emotion = emo;
            last = engine.synth_vocalization(seq, ef->intensity, fx, s, opts);
        } else if (input_mode == 2) {                // Onomatopoeia
            PhonemeMapper mapper;                    // letter-level G2P; no dict needed
            double intensity = 0.7;
            PhonemeSeq seq = onomatopoeia_to_phonemes(onomatopoeia, mapper, intensity);
            if (!cid.empty()) seq.emotion = emo;     // let the character bias the emotion
            last = engine.synth_vocalization(seq, intensity, fx, s, opts);
        } else {                                     // Line (spoken text)
            // Speak the whole line via Piper, then style it — says any words,
            // not just what's in a bank. No bank needed for speech.
            last = engine.synth_speech(text, speech_model, fx, opts, 1.0);
        }
        if (!last.ok) { status = "Synth failed: " + last.error; return false; }
        status = "Rendered " + std::to_string(last.units) + " units, peak "
               + std::to_string(last.peak_dbfs) + " dBFS";
        return true;
    };

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("grunt", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        // --- Voice bank: pick which bank to speak from ---
        ImGui::TextUnformatted("Voice bank");
        bool bank_changed = false;
        if (!banks.empty()) {
            bank_changed = ImGui::Combo("##bank", &bank_idx,
                                        bank_label_ptrs.data(), (int)bank_label_ptrs.size());
        } else {
            ImGui::TextDisabled("(no banks found — Generate one below)");
        }
        ImGui::SameLine();
        bool want_load = ImGui::Button("Load");
        // auto-load the selected bank on first frame and whenever it changes
        if (want_load || bank_changed ||
            (!engine.voice_loaded() && !banks.empty() && ImGui::GetFrameCount() == 2)) {
            std::string sel = banks.empty() ? std::string(voice_dir)
                              : voc::resource_path("voices/" + banks[bank_idx]);
            std::string err;
            if (engine.load_voice(sel, err)) {
                bool words = banks.empty() ? true : bank_has_words(banks[bank_idx]);
                status = "Loaded bank: " + engine.voice_id() +
                         (words ? "  — pick a character and Play."
                                : "  — NOTE: grunts only. Generate a voice bank to hear words.");
                if (!player_ready) player_ready = player.init(engine.sample_rate());
            } else {
                status = "Load failed: " + err;
            }
        }

        // --- Character ---
        ImGui::TextUnformatted("Character");
        ImGui::Combo("##character", &character_idx,
                     char_label_ptrs.data(), (int)char_label_ptrs.size());

        // --- Generate voices: make a word-capable bank with Piper, in-app ---
        if (ImGui::CollapsingHeader("Generate voices (make a talking bank)")) {
            if (piper_cmd.empty()) {
                ImGui::TextWrapped("Piper not found. Put piper on PATH, or place it in a "
                                   "'piper' folder next to grunt_gui.exe, then reopen. "
                                   "(setup.bat does this for you.)");
            } else {
                ImGui::TextDisabled("piper: %s", piper_cmd.c_str());
                ImGui::TextUnformatted("new bank name");
                ImGui::InputText("##genbank", gen_bank_name, sizeof(gen_bank_name));
                ImGui::Combo("voice model", &gen_model_idx, gen_models, IM_ARRAYSIZE(gen_models));
                if (ImGui::Button("Generate from examples/barks.csv")) {
                    // tell the generator which piper to use (modern python or exe)
#if defined(_WIN32)
                    _putenv_s("GRUNT_PIPER_CMD", piper_cmd.c_str());
#else
                    setenv("GRUNT_PIPER_CMD", piper_cmd.c_str(), 1);
#endif
                    GenerateOptions opt;
                    opt.units_csv     = voc::resource_path("examples/barks.csv");
                    opt.voice_dir     = voc::resource_path(std::string("voices/") + gen_bank_name);
                    opt.model_id      = gen_models[gen_model_idx];
                    opt.registry_path = voc::resource_path("data/voice_models.json");
                    opt.unit_type     = "word";
                    GenerateResult r = generate_bank(opt);
                    if (r.ok) {
                        gen_status = "Generated '" + std::string(gen_bank_name) + "': " + r.message;
                        refresh_banks();
                        // select & load the new bank
                        for (size_t i = 0; i < banks.size(); ++i)
                            if (banks[i] == gen_bank_name) { bank_idx = (int)i; break; }
                        std::string err;
                        if (engine.load_voice(opt.voice_dir, err)) {
                            if (!player_ready) player_ready = player.init(engine.sample_rate());
                            status = "Loaded generated bank: " + engine.voice_id();
                        }
                    } else {
                        gen_status = "Generate failed: " + r.error;
                    }
                }
                if (!gen_status.empty()) ImGui::TextWrapped("%s", gen_status.c_str());
            }
        }

        // Advanced: manual bank path + emotion/style overrides.
        ImGui::Checkbox("advanced", &show_advanced);
        if (show_advanced) {
            ImGui::TextUnformatted("Voice bank folder (manual path)");
            ImGui::InputText("##voice", voice_dir, sizeof(voice_dir));
            ImGui::SameLine();
            if (ImGui::Button("Load path")) {
                std::string err;
                if (engine.load_voice(voice_dir, err)) {
                    status = "Loaded bank: " + engine.voice_id();
                    if (!player_ready) player_ready = player.init(engine.sample_rate());
                } else status = "Load failed: " + err;
            }
            ImGui::Combo("emotion", &emotion_idx, emotions, IM_ARRAYSIZE(emotions));
            ImGui::Combo("style", &fx_idx, fxs, IM_ARRAYSIZE(fxs));
            ImGui::TextDisabled("(emotion/style apply only to the (none) character)");
        }

        ImGui::Separator();
        ImGui::Combo("input", &input_mode, input_modes, IM_ARRAYSIZE(input_modes));
        if (input_mode == 0) {                       // Line — spoken text
            ImGui::TextUnformatted("Line");
            ImGui::InputText("##text", text, sizeof(text));
        } else if (input_mode == 1) {                // Effort — pick from efforts.json
            ImGui::TextUnformatted("Effort");
            ImGui::Combo("##effort", &effort_idx, effort_label_ptrs.data(),
                         (int)effort_label_ptrs.size());
        } else {                                     // Onomatopoeia — free text
            ImGui::TextUnformatted("Onomatopoeia");
            ImGui::InputText("##onomat", onomatopoeia, sizeof(onomatopoeia));
            ImGui::TextDisabled("type a sound; repeat letters for intensity (argh -> aaargh)");
        }

        ImGui::Checkbox("lock seed", &lock_seed);
        if (lock_seed) { ImGui::SameLine(); ImGui::InputInt("##seed", &seed); }

        ImGui::Separator();
        if (ImGui::Button("Play") && do_synth()) {
            player.play(last.audio.samples);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) player.stop();

        ImGui::Separator();
        ImGui::TextUnformatted("Export");
        ImGui::InputText("##export", export_path, sizeof(export_path));
        ImGui::Combo("format", &format_idx, formats, IM_ARRAYSIZE(formats));
        if (ImGui::Button("Export")) {
            if (do_synth()) {
                AudioFormat fmt = (format_idx == 0) ? AudioFormat::Ogg : AudioFormat::Wav;
                if (fmt == AudioFormat::Ogg && !ogg_supported()) {
                    status = "This build lacks libvorbis — switch format to wav.";
                } else {
                    // create the parent directory if the path includes one, so
                    // "out/line.ogg" and the like don't fail to open.
                    std::error_code ec;
                    std::filesystem::path p(export_path);
                    if (p.has_parent_path() && !p.parent_path().empty())
                        std::filesystem::create_directories(p.parent_path(), ec);
                    std::string err;
                    if (write_audio(export_path, last.audio, fmt, 0.4f, err))
                        status = std::string("Exported: ")
                               + std::filesystem::absolute(p, ec).string();
                    else
                        status = "Export failed: " + err;
                }
            }
        }

        ImGui::Separator();
        ImGui::TextWrapped("%s", status.c_str());

        ImGui::End();

        ImGui::Render();
        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    if (player_ready) player.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
