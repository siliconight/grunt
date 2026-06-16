#include "Stages.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace voc {

namespace {
std::string upper(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (char c : s) o += (char)std::toupper((unsigned char)c);
    return o;
}
} // namespace

bool PhonemeMapper::load_dictionary(const std::string& path, std::string& err) {
    std::ifstream f(path);
    if (!f) { err = "cannot open dictionary: " + path; return false; }
    dict_.clear();
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string word;
        ss >> word;
        if (word.empty()) continue;
        // CMUdict marks alternate pronunciations as WORD(1), WORD(2) — keep base only
        auto paren = word.find('(');
        if (paren != std::string::npos) {
            std::string base = word.substr(0, paren);
            if (dict_.count(upper(base))) continue; // keep first pronunciation
            word = base;
        }
        std::vector<std::string> phones;
        std::string p;
        while (ss >> p) {
            // strip stress digits (AH0 -> AH) — Phase 1 doesn't model lexical stress yet
            while (!p.empty() && std::isdigit((unsigned char)p.back())) p.pop_back();
            if (!p.empty()) phones.push_back(p);
        }
        if (!phones.empty()) dict_[upper(word)] = std::move(phones);
    }
    return true;
}

// Compact rule-based English grapheme->phoneme fallback. This is deliberately
// approximate — it exists so out-of-dictionary words still produce plausible
// phonemes rather than nothing. Digraphs are checked before single letters.
std::vector<std::string> PhonemeMapper::g2p_fallback(const std::string& word) const {
    std::string w; w.reserve(word.size());
    for (char c : word) if (std::isalpha((unsigned char)c)) w += (char)std::tolower((unsigned char)c);
    std::vector<std::string> out;
    size_t i = 0, n = w.size();

    auto starts = [&](const char* s) {
        size_t len = std::char_traits<char>::length(s);
        return i + len <= n && w.compare(i, len, s) == 0;
    };

    while (i < n) {
        // --- digraphs / common clusters first ---
        if (starts("tch")) { out.push_back("CH"); i += 3; continue; }
        if (starts("sch")) { out.push_back("SH"); i += 3; continue; }
        if (starts("ch"))  { out.push_back("CH"); i += 2; continue; }
        if (starts("sh"))  { out.push_back("SH"); i += 2; continue; }
        if (starts("th"))  { out.push_back("TH"); i += 2; continue; }
        if (starts("ph"))  { out.push_back("F");  i += 2; continue; }
        if (starts("wh"))  { out.push_back("W");  i += 2; continue; }
        if (starts("ck"))  { out.push_back("K");  i += 2; continue; }
        if (starts("ng"))  { out.push_back("NG"); i += 2; continue; }
        if (starts("qu"))  { out.push_back("K"); out.push_back("W"); i += 2; continue; }
        if (starts("oo"))  { out.push_back("UW"); i += 2; continue; }
        if (starts("ee"))  { out.push_back("IY"); i += 2; continue; }
        if (starts("ea"))  { out.push_back("IY"); i += 2; continue; }
        if (starts("ai") || starts("ay")) { out.push_back("EY"); i += 2; continue; }
        if (starts("oa"))  { out.push_back("OW"); i += 2; continue; }
        if (starts("ow"))  { out.push_back("OW"); i += 2; continue; }
        if (starts("ou"))  { out.push_back("AW"); i += 2; continue; }
        if (starts("oi") || starts("oy")) { out.push_back("OY"); i += 2; continue; }
        if (starts("igh")) { out.push_back("AY"); i += 3; continue; }

        char c = w[i];
        char nx = (i + 1 < n) ? w[i + 1] : '\0';
        bool last = (i + 1 == n);

        switch (c) {
            // vowels (single)
            case 'a': out.push_back("AE"); break;
            case 'e': if (!last) out.push_back("EH"); break; // silent final e
            case 'i': out.push_back("IH"); break;
            case 'o': out.push_back("AA"); break;
            case 'u': out.push_back("AH"); break;
            case 'y': out.push_back(i == 0 ? "Y" : "IY"); break;
            // consonants
            case 'b': out.push_back("B"); break;
            case 'c': out.push_back((nx=='e'||nx=='i'||nx=='y') ? "S" : "K"); break;
            case 'd': out.push_back("D"); break;
            case 'f': out.push_back("F"); break;
            case 'g': out.push_back((nx=='e'||nx=='i'||nx=='y') ? "JH" : "G"); break;
            case 'h': out.push_back("HH"); break;
            case 'j': out.push_back("JH"); break;
            case 'k': out.push_back("K"); break;
            case 'l': out.push_back("L"); break;
            case 'm': out.push_back("M"); break;
            case 'n': out.push_back("N"); break;
            case 'p': out.push_back("P"); break;
            case 'q': out.push_back("K"); break;
            case 'r': out.push_back("R"); break;
            case 's': out.push_back("S"); break;
            case 't': out.push_back("T"); break;
            case 'v': out.push_back("V"); break;
            case 'w': out.push_back("W"); break;
            case 'x': out.push_back("K"); out.push_back("S"); break;
            case 'z': out.push_back("Z"); break;
            default: break;
        }
        ++i;
    }
    if (out.empty()) out.push_back("AH"); // never emit nothing
    return out;
}

WordPhonemes PhonemeMapper::map_word(const std::string& word) const {
    WordPhonemes wp;
    wp.word = word;
    auto it = dict_.find(upper(word));
    if (it != dict_.end()) {
        wp.phonemes = it->second;
        wp.source = PhonemeSource::Dictionary;
    } else {
        wp.phonemes = g2p_fallback(word);
        wp.source = PhonemeSource::RuleFallback;
    }
    return wp;
}

PhonemeSeq PhonemeMapper::map(const NormalizedText& nt) const {
    PhonemeSeq seq;
    seq.emotion = nt.emotion_hint;
    seq.terminal_punct = nt.terminal_punct;
    for (const auto& tok : nt.tokens) {
        WordPhonemes wp = map_word(tok);
        for (const auto& e : nt.emphasis_words) if (e == tok) { wp.is_emphasis = true; break; }
        seq.words.push_back(std::move(wp));
    }
    return seq;
}

} // namespace voc
