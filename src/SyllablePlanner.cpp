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

// Build the word-first request for a single token: a Word-preferred slot whose
// fallback chain degrades syllable -> phoneme -> grunt. Factored out so both the
// plain and phrase-aware planners share identical per-word behaviour.
static RequestedUnit build_word_request(const std::string& tok, bool emph,
                                        const PhonemeMapper& mapper) {
    WordPhonemes wp = mapper.map_word(tok);
    auto sylls = syllabify_phonemes(wp.phonemes);

    std::string wkey = tok;
    for (auto& c : wkey) c = (char)std::tolower((unsigned char)c);

    RequestedUnit word;
    word.key = wkey;                 // matches a generate-baked word unit
    word.preferred = UnitType::Word;
    for (auto& syl : sylls) {
        std::string sk;
        for (size_t i = 0; i < syl.size(); ++i) sk += (i ? " " : "") + syl[i];
        word.fallback.push_back(sk);
    }
    for (const auto& p : wp.phonemes) word.fallback.push_back(p);
    word.fallback.push_back("");     // grunt
    word.is_emphasis = emph;
    return word;
}

UnitPlan SyllablePlanner::plan_phonemic(const NormalizedText& nt,
                                        const PhonemeMapper& mapper) const {
    UnitPlan up;
    up.emotion = nt.emotion_hint;
    up.terminal_punct = nt.terminal_punct;

    for (const auto& tok : nt.tokens) {
        bool emph = false;
        for (const auto& e : nt.emphasis_words) if (e == tok) { emph = true; break; }
        // WORD-FIRST: a baked word unit wins when present -> crisp speech;
        // otherwise degrades syllable -> phoneme -> grunt.
        up.units.push_back(build_word_request(tok, emph, mapper));
    }
    return up;
}

UnitPlan SyllablePlanner::plan_phonemic(const NormalizedText& nt,
                                        const PhonemeMapper& mapper,
                                        const UnitDatabase& db) const {
    UnitPlan up;
    up.emotion = nt.emotion_hint;
    up.terminal_punct = nt.terminal_punct;

    // PHRASE-FIRST (limited-domain synthesis): walk the tokens left to right;
    // at each position try the LONGEST run of tokens that the bank has baked as
    // a single phrase unit (keyed by the lowercased space-joined words). A whole
    // phrase = one unit, zero internal joins -> highest quality. When no phrase
    // matches at this position, emit one word request (word -> syllable ->
    // phoneme -> grunt) and advance by one. This mirrors the longest-segment
    // decomposition from limited-domain TTS literature.
    const auto& toks = nt.tokens;
    size_t i = 0;
    while (i < toks.size()) {
        size_t matched_len = 0;
        std::string matched_key;
        // greedy longest: try the whole remaining run, shrink from the end.
        for (size_t len = toks.size() - i; len >= 2; --len) {
            std::string key;
            for (size_t k = 0; k < len; ++k) {
                if (k) key += " ";
                std::string w = toks[i + k];
                for (auto& c : w) c = (char)std::tolower((unsigned char)c);
                key += w;
            }
            if (!db.match_key(key).empty()) { matched_len = len; matched_key = key; break; }
        }

        if (matched_len >= 2) {
            // emit a single phrase slot. Fallback: the first word's breakdown,
            // so a bank that lost the phrase still degrades gracefully on this
            // slot; the remaining words of the phrase are then planned normally
            // below only if the phrase unit is absent at selection time. To keep
            // the slot count honest we emit the phrase as ONE unit and let the
            // remaining tokens be re-planned per-word (the phrase wins when baked).
            bool emph = false;
            for (const auto& e : nt.emphasis_words)
                if (e == toks[i]) { emph = true; break; }
            RequestedUnit phrase;
            phrase.key = matched_key;
            phrase.preferred = UnitType::Phrase;
            // graceful degradation: if the phrase unit is missing, fall back to
            // the first word, then its phoneme(s), then grunt — a reasonable
            // single-slot approximation. (Full per-word re-expansion is a future
            // refinement; the phrase unit is the intended path.)
            RequestedUnit w0 = build_word_request(toks[i], emph, mapper);
            phrase.fallback.push_back(w0.key);
            for (const auto& f : w0.fallback) phrase.fallback.push_back(f);
            phrase.is_emphasis = emph;
            up.units.push_back(phrase);
            i += matched_len;
        } else {
            bool emph = false;
            for (const auto& e : nt.emphasis_words)
                if (e == toks[i]) { emph = true; break; }
            up.units.push_back(build_word_request(toks[i], emph, mapper));
            i += 1;
        }
    }
    return up;
}

} // namespace voc
