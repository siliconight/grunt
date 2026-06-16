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
            ru.preferred = UnitType::Syllable;
            ru.fallback = {""};   // grunt fallback (empty key = any grunt)
            ru.is_emphasis = emph;
            up.units.push_back(ru);
        }
    }
    return up;
}

// ARPAbet vowels — syllable nuclei.
static bool is_arpabet_vowel(const std::string& p) {
    static const char* v[] = {"AA","AE","AH","AO","AW","AY","EH","ER","EY",
                              "IH","IY","OW","OY","UH","UW"};
    for (const char* x : v) if (p == x) return true;
    return false;
}

// Group a word's phonemes into syllables: each syllable is (optional onset
// consonants)(vowel nucleus)(coda consonants up to the next onset). Simple
// max-onset-ish rule: a single consonant between vowels starts the next
// syllable; runs split leaving one for the next onset.
static std::vector<std::vector<std::string>>
syllabify_phonemes(const std::vector<std::string>& ph) {
    std::vector<std::vector<std::string>> sylls;
    std::vector<std::string> cur;
    bool have_nucleus = false;
    for (size_t i = 0; i < ph.size(); ++i) {
        bool vowel = is_arpabet_vowel(ph[i]);
        if (vowel) {
            if (have_nucleus) {
                // new vowel: close current syllable, but if the last item was a
                // single consonant, move it to the new onset.
                if (!cur.empty() && !is_arpabet_vowel(cur.back())) {
                    std::string onset = cur.back();
                    cur.pop_back();
                    if (!cur.empty()) sylls.push_back(cur);
                    cur.clear();
                    cur.push_back(onset);
                } else {
                    sylls.push_back(cur);
                    cur.clear();
                }
            }
            cur.push_back(ph[i]);
            have_nucleus = true;
        } else {
            cur.push_back(ph[i]);
        }
    }
    if (!cur.empty()) {
        if (have_nucleus || sylls.empty()) sylls.push_back(cur);
        else { for (auto& p : cur) sylls.back().push_back(p); } // trailing cons
    }
    return sylls;
}

UnitPlan SyllablePlanner::plan_phonemic(const NormalizedText& nt,
                                        const PhonemeMapper& mapper) const {
    UnitPlan up;
    up.emotion = nt.emotion_hint;
    up.terminal_punct = nt.terminal_punct;

    for (const auto& tok : nt.tokens) {
        bool emph = false;
        for (const auto& e : nt.emphasis_words) if (e == tok) { emph = true; break; }

        WordPhonemes wp = mapper.map_word(tok);
        auto sylls = syllabify_phonemes(wp.phonemes);
        if (sylls.empty()) {
            // no phonemes -> grunt-only unit
            RequestedUnit ru; ru.key = tok; ru.preferred = UnitType::Syllable;
            ru.fallback = {""}; ru.is_emphasis = emph;
            up.units.push_back(ru);
            continue;
        }
        for (auto& syl : sylls) {
            RequestedUnit ru;
            // syllable key: phonemes joined by space (matches how a bank keys
            // syllable units), lowercased for case-insensitive bank matching.
            std::string key;
            for (size_t i = 0; i < syl.size(); ++i) key += (i ? " " : "") + syl[i];
            ru.key = key;
            ru.preferred = UnitType::Syllable;
            // fallback chain: each constituent phoneme, then a grunt
            for (const auto& p : syl) ru.fallback.push_back(p);
            ru.fallback.push_back("");   // grunt
            ru.is_emphasis = emph;
            up.units.push_back(ru);
        }
    }
    return up;
}

} // namespace voc
