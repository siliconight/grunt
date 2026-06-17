#include "BankGen.h"
#include "Generator.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <cctype>

namespace voc {

namespace {
// minimal JSON string escaper (same rules as the CLI used)
std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\t': o += "\\t";  break;
            default:   o += c;      break;
        }
    }
    return o;
}
void emit(const std::function<void(const std::string&)>& log, const std::string& m) {
    if (log) log(m);
}
} // namespace

GenerateResult generate_bank(const GenerateOptions& opt,
                             const std::function<void(const std::string&)>& log) {
    GenerateResult r;

    if (opt.unit_type != "word" && opt.unit_type != "syllable") {
        r.error = "unit_type must be 'word' or 'syllable'"; return r;
    }

    VoiceModelRegistry reg; std::string err;
    if (!reg.load(opt.registry_path, err)) { r.error = "registry: " + err; return r; }

    const VoiceModel* model = reg.find(opt.model_id);
    if (!model) { r.error = "model '" + opt.model_id + "' not in registry"; return r; }

    std::string gen_name = opt.generator.empty() ? model->generator : opt.generator;
    Generator* gen = make_generator(gen_name);
    if (!gen) { r.error = "unknown generator: " + gen_name; return r; }

    std::string units_dir = opt.voice_dir + "/units/generated";
    std::error_code ec;
    std::filesystem::create_directories(units_dir, ec);
    if (ec) { r.error = "cannot create " + units_dir + ": " + ec.message(); delete gen; return r; }

    std::ifstream csv(opt.units_csv);
    if (!csv) { r.error = "cannot open units csv: " + opt.units_csv; delete gen; return r; }

    std::ostringstream units_json;
    units_json << "{\n  \"units\": [\n";

    std::string line; bool first = true; int n = 0, blocked = 0; bool hdr = false;
    while (std::getline(csv, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // tolerate CRLF
        if (line.empty()) continue;
        std::vector<std::string> f; std::stringstream ss(line); std::string cell;
        while (std::getline(ss, cell, ',')) f.push_back(cell);
        if (f.size() < 2) continue;
        if (!hdr && f[0] == "key") { hdr = true; continue; }
        hdr = true;

        std::string key = f[0], text = f[1];
        GeneratedClip clip = gen->generate(key, text, *model, units_dir);
        if (!clip.ok) { emit(log, "skip " + key + ": " + clip.error); continue; }

        if (!clip.provenance.commercial_use || clip.provenance.synth_tool_derived) {
            emit(log, "note: " + key + " is not shippable — gate will block it");
            blocked++;
        }

        std::string ukey, utype;
        if (opt.unit_type == "syllable") {
            ukey = key; utype = "syllable";
        } else {
            ukey = text;
            for (auto& c : ukey) c = (char)std::tolower((unsigned char)c);
            while (!ukey.empty() && ukey.front() == ' ') ukey.erase(ukey.begin());
            while (!ukey.empty() && ukey.back() == ' ') ukey.pop_back();
            utype = "word";
        }

        std::string rel = "units/generated/" + key + ".wav";
        if (!first) units_json << ",\n";
        first = false;
        units_json << "    { \"id\": \"gen_" << json_escape(key) << "\""
                   << ", \"type\": \"" << utype << "\", \"key\": \"" << json_escape(ukey) << "\""
                   << ", \"emotion\": \"neutral\", \"file\": \"" << json_escape(rel) << "\""
                   << ", \"provenance\": {"
                   << " \"source\": \"" << json_escape(clip.provenance.source) << "\""
                   << ", \"recorded_by\": \"" << json_escape(clip.provenance.recorded_by) << "\""
                   << ", \"license\": \"" << json_escape(clip.provenance.license) << "\""
                   << ", \"commercial_use\": " << (clip.provenance.commercial_use ? "true" : "false")
                   << ", \"synth_tool_derived\": " << (clip.provenance.synth_tool_derived ? "true" : "false")
                   << " } }";
        n++;
        emit(log, "baked " + key);
    }
    units_json << "\n  ]\n}\n";

    std::filesystem::create_directories(opt.voice_dir + "/metadata", ec);
    std::ofstream mf(opt.voice_dir + "/metadata/units.json");
    mf << units_json.str();

    // Write the bank descriptor (voice.json). Without it the UnitDatabase loader
    // refuses the bank ("missing or empty voice.json"), so a freshly generated
    // bank couldn't be loaded for synth/preview even though its clips are baked.
    // voice_id = the bank's folder name; sample_rate matches piper's 22050 output.
    {
        std::string bank_name =
            std::filesystem::path(opt.voice_dir).filename().string();
        if (bank_name.empty())
            bank_name = std::filesystem::path(opt.voice_dir).parent_path().filename().string();
        std::ofstream vf(opt.voice_dir + "/voice.json");
        vf << "{\n"
           << "  \"voice_id\": \"" << json_escape(bank_name) << "\",\n"
           << "  \"display_name\": \"" << json_escape(bank_name) << "\",\n"
           << "  \"schema_version\": 1,\n"
           << "  \"sample_rate\": 22050,\n"
           << "  \"base_pitch\": 0,\n"
           << "  \"default_fx_preset\": \"clean_ps1\",\n"
           << "  \"generated_by\": \"grunt generate (" << json_escape(gen_name)
           << ", model " << json_escape(model->id) << ")\"\n"
           << "}\n";
    }
    delete gen;

    if (n == 0) { r.error = "no units were generated (empty or unreadable CSV?)"; return r; }

    r.ok = true; r.units = n; r.blocked = blocked;
    r.message = "generated " + std::to_string(n) + " units via " + gen_name
              + " (model " + model->id + ")"
              + (blocked ? "; " + std::to_string(blocked) + " NOT shippable (gate will block)"
                         : "; all gate-clean");
    return r;
}

} // namespace voc
