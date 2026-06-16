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

    // window/taskbar icon (optional — silently skipped if the file isn't found)
    {
        int iw, ih, ic;
        unsigned char* px = stbi_load("assets/grunt_icon64.png", &iw, &ih, &ic, 4);
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
    std::string status = "Load a voice bank to begin.";
    char voice_dir[256];
    std::snprintf(voice_dir, sizeof(voice_dir), "%s",
                  voc::resource_path("voices/heavy_brother").c_str());
    char text[256] = "Open the gate!";
    int emotion_idx = 0;   // 0 neutral, 1 urgent, 2 angry
    int fx_idx = 0;
    int seed = 42;
    bool lock_seed = true; // deterministic by default
    char export_path[256] = "out/line.ogg";
    int format_idx = 0;    // 0 ogg, 1 wav

    const char* emotions[] = { "neutral", "urgent", "angry" };
    const char* fxs[] = { "clean_ps1", "radio_ps1", "monster_ps1", "robot_ps1", "muffled_mask" };
    const char* formats[] = { "ogg (Vorbis)", "wav" };

    bool player_ready = false;
    SynthResult last; // cached last render for export

    auto do_synth = [&]() -> bool {
        if (!engine.voice_loaded()) { status = "No voice bank loaded."; return false; }
        uint64_t s = lock_seed ? (uint64_t)seed
                               : (uint64_t)glfwGetTime() * 1000003ULL + 1;
        last = engine.synth(text, (Emotion)emotion_idx, fxs[fx_idx], s);
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

        ImGui::TextUnformatted("Voice bank");
        ImGui::InputText("##voice", voice_dir, sizeof(voice_dir));
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            std::string err;
            if (engine.load_voice(voice_dir, err)) {
                status = "Loaded voice: " + engine.voice_id();
                if (!player_ready) player_ready = player.init(engine.sample_rate());
            } else {
                status = "Load failed: " + err;
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Line");
        ImGui::InputText("##text", text, sizeof(text));

        ImGui::Combo("emotion", &emotion_idx, emotions, IM_ARRAYSIZE(emotions));
        ImGui::Combo("style", &fx_idx, fxs, IM_ARRAYSIZE(fxs));

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
                    std::string err;
                    if (write_audio(export_path, last.audio, fmt, 0.4f, err))
                        status = std::string("Exported: ") + export_path;
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
