#pragma once
#include <string>

// Punctuation -> inflection helpers. Piper derives prosody almost entirely from
// the text + its punctuation; grunt's default leaves it conservative. "Punchy"
// mode rewrites a line so simple punctuation drives stronger, more dramatic
// delivery, and derives a per-line --sentence-silence to match.
//
// All OFF by default (punchy=false is a no-op that returns the text unchanged),
// so the baseline sound is never altered unless the user opts in.
namespace voc {

// Rewrite `line` so its punctuation reads more expressively when punchy is on.
// Rules (punchy=true):
//   - a line with NO terminal punctuation gets a period (flat -> real contour)
//   - "..." is normalized to a clean trailing ellipsis (drawn-out)
//   - runs of spaces collapse; ensures a single space after , . ! ? so the
//     marks actually register
// Conservative on purpose: it does NOT split sentences or invent punctuation
// beyond a terminal period. punchy=false returns the input unchanged.
std::string punchify_text(const std::string& line, bool punchy);

// Derive --sentence-silence (seconds) for a line from its punctuation.
// punchy=false returns a negative value meaning "don't pass the flag" (Piper
// default). punchy=true returns a stronger, more dramatic value, larger when the
// line trails off (ellipsis) or stacks multiple sentences.
double sentence_silence_for(const std::string& line, bool punchy);

} // namespace voc
