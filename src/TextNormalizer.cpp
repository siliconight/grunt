#include "Stages.h"
#include <cctype>

namespace voc {

NormalizedText TextNormalizer::normalize(const std::string& text) const {
    NormalizedText nt;

    // detect terminal punctuation and emotion hint
    bool has_excl = text.find('!') != std::string::npos;
    bool has_q    = text.find('?') != std::string::npos;
    if (has_excl && has_q) { nt.terminal_punct = "?!"; nt.emotion_hint = Emotion::Urgent; }
    else if (has_excl)     { nt.terminal_punct = "!";  nt.emotion_hint = Emotion::Urgent; }
    else if (has_q)        { nt.terminal_punct = "?"; }
    else if (text.find('.') != std::string::npos) { nt.terminal_punct = "."; }

    // tokenize: keep letters/digits, split on everything else, track CAPS emphasis
    std::string cur;
    bool cur_upper = true;
    bool cur_has_alpha = false;
    auto flush = [&]() {
        if (cur.empty()) return;
        std::string lower; lower.reserve(cur.size());
        for (char c : cur) lower += (char)std::tolower((unsigned char)c);
        nt.tokens.push_back(lower);
        if (cur_has_alpha && cur_upper && cur.size() > 1) nt.emphasis_words.push_back(lower);
        cur.clear(); cur_upper = true; cur_has_alpha = false;
    };

    for (char c : text) {
        if (std::isalnum((unsigned char)c)) {
            cur += c;
            if (std::isalpha((unsigned char)c)) {
                cur_has_alpha = true;
                if (std::islower((unsigned char)c)) cur_upper = false;
            }
        } else {
            flush();
        }
    }
    flush();

    return nt;
}

} // namespace voc
