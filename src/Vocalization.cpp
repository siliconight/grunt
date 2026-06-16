#include "Vocalization.h"
#include "Json.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace voc {

bool EffortLibrary::load(const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open efforts file: " + path; return false; }
    std::stringstream ss; ss << f.rdbuf();
    try {
        Json root = Json::parse(ss.str());
        const Json& list = root["efforts"];
        for (const auto& e : list.items()) {
            Effort ef;
            ef.id = e["id"].as_string();
            for (const auto& p : e["phonemes"].items()) ef.phonemes.push_back(p.as_string());
            ef.intensity = e["intensity"].as_number(0.7);
            ef.emotion = emotion_from_string(e["emotion"].as_string("urgent"));
            ef.desc = e["_desc"].as_string();
            efforts_.push_back(std::move(ef));
        }
    } catch (const std::exception& ex) {
        err = std::string("efforts parse: ") + ex.what(); return false;
    }
    return true;
}

const Effort* EffortLibrary::find(const std::string& id) const {
    for (const auto& e : efforts_) if (e.id == id) return &e;
    return nullptr;
}

PhonemeSeq effort_to_phonemes(const Effort& e) {
    PhonemeSeq seq;
    seq.emotion = e.emotion;
    seq.terminal_punct = "!";   // efforts are exclamatory by nature
    WordPhonemes wp;
    wp.word = e.id;
    wp.phonemes = e.phonemes;
    wp.source = PhonemeSource::Passthrough;
    wp.is_emphasis = true;       // efforts are stressed
    seq.words.push_back(std::move(wp));
    return seq;
}

// Map a single vowel letter to a sustained ARPAbet vowel for onomatopoeia.
static const char* vowel_phoneme(char c) {
    switch (std::tolower((unsigned char)c)) {
        case 'a': return "AA";   // "aaargh"
        case 'e': return "EH";
        case 'i': return "IY";
        case 'o': return "AO";   // "ooo"
        case 'u': return "AH";   // "ungh"
        case 'y': return "IY";
        default:  return nullptr;
    }
}

PhonemeSeq onomatopoeia_to_phonemes(const std::string& spelling,
                                    const PhonemeMapper& mapper,
                                    double& intensity_out) {
    PhonemeSeq seq;
    seq.emotion = Emotion::Urgent;
    seq.terminal_punct = "!";

    WordPhonemes wp;
    wp.word = spelling;
    wp.source = PhonemeSource::Passthrough;
    wp.is_emphasis = true;

    int max_run = 1;
    size_t i = 0, n = spelling.size();
    while (i < n) {
        char c = spelling[i];
        if (!std::isalpha((unsigned char)c)) { ++i; continue; }
        // count a run of the same letter
        size_t j = i;
        while (j < n && std::tolower((unsigned char)spelling[j]) == std::tolower((unsigned char)c)) ++j;
        int run = (int)(j - i);
        if (run > max_run) max_run = run;

        const char* v = vowel_phoneme(c);
        if (v) {
            // sustained vowel: emit it once per ~2 repeats so "aaaa" lengthens
            int reps = 1 + (run - 1) / 2;
            for (int k = 0; k < reps; ++k) wp.phonemes.push_back(v);
        } else {
            // consonant: voice it via the mapper's single-letter G2P
            WordPhonemes c1 = mapper.map_word(std::string(1, c));
            for (const auto& p : c1.phonemes) wp.phonemes.push_back(p);
        }
        i = j;
    }
    if (wp.phonemes.empty()) wp.phonemes.push_back("AA");

    // repeated letters -> higher intensity (clamped)
    intensity_out = 0.6 + 0.1 * (max_run - 1);
    if (intensity_out > 1.0) intensity_out = 1.0;

    seq.words.push_back(std::move(wp));
    return seq;
}

} // namespace voc
