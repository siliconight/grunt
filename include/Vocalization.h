#pragma once
// Vocalization.h — non-lexical vocalizations (TDD §8 efforts). Two front-ends
// that both produce a PhonemeSeq so the rest of the pipeline is unchanged:
//   1. named efforts (data/efforts.json): ask for "pain_death", get a scream
//   2. onomatopoeia passthrough: type "aaargh", get it voiced literally
//      (repeated/stretched letters -> intensity + duration, not word lookup)
#include "Types.h"
#include "Stages.h"   // PhonemeMapper, for letter->phoneme of onomatopoeia
#include <string>
#include <vector>

namespace voc {

struct Effort {
    std::string id;
    std::vector<std::string> phonemes;
    double intensity = 0.7;          // 0..1, scales gain + duration
    Emotion emotion = Emotion::Urgent;
    std::string desc;
};

class EffortLibrary {
public:
    bool load(const std::string& path, std::string& err);
    const Effort* find(const std::string& id) const;
    const std::vector<Effort>& all() const { return efforts_; }
private:
    std::vector<Effort> efforts_;
};

// Build a PhonemeSeq for a named effort.
PhonemeSeq effort_to_phonemes(const Effort& e);

// Parse a literal onomatopoeic spelling into a PhonemeSeq. Repeated letters
// ("aaa") raise intensity and length; the spelling is voiced as-is, NOT looked
// up as a word. Uses the mapper's letter->phoneme rules for the consonants.
PhonemeSeq onomatopoeia_to_phonemes(const std::string& spelling,
                                    const PhonemeMapper& mapper,
                                    double& intensity_out);

} // namespace voc
