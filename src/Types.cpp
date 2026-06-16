#include "Types.h"
#include <algorithm>

namespace voc {

Emotion emotion_from_string(const std::string& s) {
    std::string t; t.reserve(s.size());
    for (char c : s) t += (char)std::tolower((unsigned char)c);
    if (t == "urgent") return Emotion::Urgent;
    if (t == "angry")  return Emotion::Angry;
    return Emotion::Neutral;
}

const char* emotion_to_string(Emotion e) {
    switch (e) {
        case Emotion::Urgent: return "urgent";
        case Emotion::Angry:  return "angry";
        default:              return "neutral";
    }
}

} // namespace voc
