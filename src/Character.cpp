#include "Character.h"
#include "Json.h"
#include <fstream>
#include <sstream>

namespace voc {

bool CharacterLibrary::load(const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open characters file: " + path; return false; }
    std::stringstream ss; ss << f.rdbuf();
    try {
        Json root = Json::parse(ss.str());
        const Json& list = root["characters"];
        for (const auto& c : list.items()) {
            CharacterPreset p;
            p.id              = c["id"].as_string();
            p.display_name    = c["display_name"].as_string();
            p.base_voice      = c["base_voice"].as_string();
            p.pitch_offset_st = c["pitch_offset_st"].as_number(0.0);
            p.fx_preset       = c["fx_preset"].as_string("clean_ps1");
            p.emotion_bias    = c["emotion_bias"].as_string("neutral");
            p.gain_db         = c["gain_db"].as_number(0.0);
            p.ready           = c["ready"].as_bool(true);
            p.sub_layer       = c["sub_layer"].as_bool(false);
            p.rasp            = c["rasp"].as_bool(false);
            p.formant_shift   = c["formant_shift"].as_number(0.0);
            p.blocked_on      = c["_blocked_on"].as_string();
            presets_.push_back(std::move(p));
        }
    } catch (const std::exception& e) {
        err = std::string("characters parse: ") + e.what(); return false;
    }
    return true;
}

const CharacterPreset* CharacterLibrary::find(const std::string& id) const {
    for (const auto& p : presets_) if (p.id == id) return &p;
    return nullptr;
}

} // namespace voc
