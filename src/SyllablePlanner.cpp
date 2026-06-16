#include "Stages.h"
#include <cctype>

namespace voc {

static bool is_vowel(char c) {
    c = (char)std::tolower((unsigned char)c);
    return c=='a'||c=='e'||c=='i'||c=='o'||c=='u'||c=='y';
}

// Grunt-mode syllabifier: split a word into rough CV / CVC chunks so the
// renderer has playable, rhythm-driving units. This is intentionally crude —
// it drives timing and grunt selection, not accurate pronunciation.
std::vector<std::string> SyllablePlanner::syllabify(const std::string& word) const {
    std::vector<std::string> out;
    if (word.empty()) return out;

    std::string cur;
    bool seen_vowel = false;
    for (size_t i = 0; i < word.size(); ++i) {
        char c = word[i];
        cur += c;
        if (is_vowel(c)) {
            seen_vowel = true;
        } else if (seen_vowel) {
            // consonant after a vowel: peek — if a vowel follows, break before
            // this consonant (open syllable); otherwise keep it (closing coda)
            bool next_vowel = (i + 1 < word.size()) && is_vowel(word[i + 1]);
            if (next_vowel) {
                cur.pop_back();
                out.push_back(cur);
                cur = std::string(1, c);
                seen_vowel = false;
            }
        }
    }
    if (!cur.empty()) {
        if (out.empty() || seen_vowel) out.push_back(cur);
        else out.back() += cur; // trailing consonants with no vowel -> attach
    }
    return out;
}

UnitPlan SyllablePlanner::plan(const NormalizedText& nt) const {
    UnitPlan up;
    up.emotion = nt.emotion_hint;
    up.terminal_punct = nt.terminal_punct;

    for (const auto& tok : nt.tokens) {
        bool emph = false;
        for (const auto& e : nt.emphasis_words) if (e == tok) { emph = true; break; }
        for (auto& syl : syllabify(tok)) {
            RequestedUnit ru;
            ru.key = syl;
            ru.is_emphasis = emph;
            up.units.push_back(ru);
        }
    }
    return up;
}

} // namespace voc
