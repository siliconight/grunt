#pragma once
// ResourcePath.h — resolve bundled resources (data/, voices/) regardless of the
// working directory. A double-clicked GUI launches from an arbitrary CWD, so
// resources must be found relative to the executable, not the shell's PWD.
//
// Search order for a relative resource like "data/characters.json":
//   1. <exe_dir>/<rel>            (packaged layout: data/ next to the binary)
//   2. <exe_dir>/../<rel>         (build/ layout: binary in build/, data/ above)
//   3. <exe_dir>/../share/grunt/<rel>  (unix install layout)
//   4. <rel>                      (CWD: running from the repo root)
// First existing match wins; if none exist, returns <rel> unchanged so callers
// still get a sensible path for error messages.
#include <string>
#include <filesystem>

namespace voc {

// Set once at startup from argv[0] so resolution knows where the binary lives.
void set_exe_path(const char* argv0);

// Resolve a relative resource path to an absolute one that exists, per the
// search order above. Accepts files or directories.
std::string resource_path(const std::string& rel);

} // namespace voc
