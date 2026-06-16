#include "Stages.h"
#include "Json.h"
#include <fstream>
#include <sstream>

namespace voc {

static UnitType type_from_string(const std::string& s) {
    if (s == "phoneme")  return UnitType::Phoneme;
    if (s == "diphone")  return UnitType::Diphone;
    if (s == "syllable") return UnitType::Syllable;
    if (s == "word")     return UnitType::Word;
    if (s == "effort")   return UnitType::Effort;
    return UnitType::Grunt;
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

bool UnitDatabase::load(const std::string& voice_dir, std::string& err) {
    dir_ = voice_dir;

    // voice.json
    std::string vtext = read_file(voice_dir + "/voice.json");
    if (vtext.empty()) { err = "missing or empty voice.json in " + voice_dir; return false; }
    try {
        Json v = Json::parse(vtext);
        voice_id_   = v["voice_id"].as_string();
        sample_rate_ = (int)v["sample_rate"].as_number(22050);
        base_pitch_  = v["base_pitch"].as_number(0.0);
    } catch (const std::exception& e) {
        err = std::string("voice.json: ") + e.what(); return false;
    }

    // metadata/units.json
    std::string utext = read_file(voice_dir + "/metadata/units.json");
    if (utext.empty()) { err = "missing or empty metadata/units.json in " + voice_dir; return false; }
    try {
        Json arr = Json::parse(utext);
        const Json& list = arr.is_array() ? arr : arr["units"];
        for (const auto& j : list.items()) {
            AudioUnit u;
            u.id   = j["id"].as_string();
            u.type = type_from_string(j["type"].as_string("grunt"));
            u.key  = j["key"].as_string();
            u.emotion = emotion_from_string(j["emotion"].as_string("neutral"));
            u.pitch_center_hz = j["pitch_center_hz"].as_number(0.0);
            u.duration_ms = (int)j["duration_ms"].as_number(0);
            u.energy = j["energy"].as_number(0.0);
            u.file = j["file"].as_string();
            if (j.has("provenance")) {
                const Json& p = j["provenance"];
                u.provenance.source = p["source"].as_string("original_recording");
                u.provenance.recorded_by = p["recorded_by"].as_string();
                u.provenance.license = p["license"].as_string("owned");
                u.provenance.commercial_use = p["commercial_use"].as_bool(true);
                u.provenance.synth_tool_derived = p["synth_tool_derived"].as_bool(false);
            }
            units_.push_back(std::move(u));
        }
    } catch (const std::exception& e) {
        err = std::string("units.json: ") + e.what(); return false;
    }

    if (units_.empty()) { err = "voice bank has no units: " + voice_dir; return false; }
    return true;
}

std::vector<const AudioUnit*> UnitDatabase::candidates(const std::string& key,
                                                       Emotion emotion) const {
    std::vector<const AudioUnit*> exact, grunts;
    for (const auto& u : units_) {
        if (u.type == UnitType::Grunt) grunts.push_back(&u);
        if (u.key == key) exact.push_back(&u);
    }
    (void)emotion; // emotion weighting handled in selector cost
    // prefer exact key matches; always allow grunts as universal fallback
    std::vector<const AudioUnit*> out = exact;
    out.insert(out.end(), grunts.begin(), grunts.end());
    return out;
}

} // namespace voc
