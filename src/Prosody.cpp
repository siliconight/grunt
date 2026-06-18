#include "Prosody.h"
#include <cctype>

namespace voc {

static bool ends_with_terminal(const std::string& s) {
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        if (std::isspace((unsigned char)*it)) continue;
        char c = *it;
        return c == '.' || c == '!' || c == '?';
    }
    return false;  // empty/whitespace-only
}

std::string punchify_text(const std::string& line, bool punchy) {
    if (!punchy) return line;

    // 1. collapse whitespace runs and ensure a single space after , . ! ?
    std::string out;
    out.reserve(line.size() + 4);
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (std::isspace((unsigned char)c)) {
            if (!out.empty() && !std::isspace((unsigned char)out.back()))
                out.push_back(' ');
            continue;
        }
        // keep an ellipsis run intact ("..."): copy all consecutive dots, then
        // ensure a single trailing space — never split into ". . ."
        if (c == '.' && i + 1 < line.size() && line[i + 1] == '.') {
            while (i < line.size() && line[i] == '.') { out.push_back('.'); ++i; }
            --i;  // for-loop will ++ back to the char after the dots
            if (i + 1 < line.size() && !std::isspace((unsigned char)line[i + 1]))
                out.push_back(' ');
            continue;
        }
        out.push_back(c);
        // after a mid-line punctuation mark, guarantee a separating space
        if ((c == ',' || c == '.' || c == '!' || c == '?') &&
            i + 1 < line.size() && !std::isspace((unsigned char)line[i + 1]))
            out.push_back(' ');
    }
    // trim trailing space
    while (!out.empty() && std::isspace((unsigned char)out.back())) out.pop_back();
    if (out.empty()) return line;

    // 2. a line with no terminal punctuation reads flat -> give it a period so
    //    Piper applies a real falling sentence contour instead of trailing off.
    if (!ends_with_terminal(out)) out.push_back('.');

    return out;
}

double sentence_silence_for(const std::string& line, bool punchy) {
    if (!punchy) return -1.0;  // sentinel: caller omits the flag (Piper default)

    // Stronger / more dramatic baseline than Piper's default (~0.2s).
    double sil = 0.35;

    // Trailing-off lines (ellipsis) hang longer for menace/hesitation.
    if (line.find("...") != std::string::npos) sil = 0.6;

    // Count sentence-ending marks; multi-sentence barks want clear beats.
    int terminals = 0;
    for (char c : line) if (c == '.' || c == '!' || c == '?') ++terminals;
    if (terminals >= 2) sil += 0.1;

    // Cap so it never drags absurdly.
    if (sil > 0.8) sil = 0.8;
    return sil;
}

} // namespace voc
