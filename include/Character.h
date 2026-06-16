#pragma once
// Character.h — named character presets (data/characters.json). A preset is a
// recipe over the pipeline: base voice + pitch + FX preset + emotion bias
// (+ planned formant/sub/rasp). Pick a character by name; grunt applies the
// recipe. Most characters layer over a few shared base voices.
#include "Types.h"
#include <string>
#include <vector>

namespace voc {

struct CharacterPreset {
    std::string id;
    std::string display_name;
    std::string base_voice;        // model id in voice_models.json
    double pitch_offset_st = 0.0;
    std::string fx_preset = "clean_ps1";
    std::string emotion_bias = "neutral";
    double gain_db = 0.0;
    bool ready = true;             // false = needs DSP not yet built
    // planned DSP (ignored until the renderer supports them)
    bool sub_layer = false;
    bool rasp = false;
    double formant_shift = 0.0;
    std::string blocked_on;
};

class CharacterLibrary {
public:
    bool load(const std::string& path, std::string& err);
    const CharacterPreset* find(const std::string& id) const;
    const std::vector<CharacterPreset>& all() const { return presets_; }
private:
    std::vector<CharacterPreset> presets_;
};

} // namespace voc
