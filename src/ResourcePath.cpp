#include "ResourcePath.h"
#include <system_error>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__APPLE__)
  #include <mach-o/dyld.h>
#else
  #include <unistd.h>
#endif

namespace voc {

namespace {
std::filesystem::path g_exe_dir;   // directory containing the executable

// Best-effort: the OS path to this executable. Falls back to argv0.
std::filesystem::path discover_exe(const char* argv0) {
#if defined(_WIN32)
    wchar_t buf[1024];
    DWORD n = GetModuleFileNameW(nullptr, buf, 1024);
    if (n > 0 && n < 1024) return std::filesystem::path(std::wstring(buf, n));
#elif defined(__APPLE__)
    char buf[4096]; uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) == 0) return std::filesystem::path(buf);
#else
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) return p;
#endif
    if (argv0 && *argv0) {
        std::error_code ec;
        auto p = std::filesystem::absolute(argv0, ec);
        if (!ec) return p;
    }
    return {};
}
} // namespace

void set_exe_path(const char* argv0) {
    auto exe = discover_exe(argv0);
    if (!exe.empty()) {
        std::error_code ec;
        g_exe_dir = std::filesystem::weakly_canonical(exe, ec).parent_path();
        if (g_exe_dir.empty()) g_exe_dir = exe.parent_path();
    }
}

std::string resource_path(const std::string& rel) {
    std::error_code ec;
    if (!g_exe_dir.empty()) {
        const std::filesystem::path bases[] = {
            g_exe_dir / rel,
            g_exe_dir / ".." / rel,
            g_exe_dir / ".." / "share" / "grunt" / rel,
        };
        for (const auto& b : bases)
            if (std::filesystem::exists(b, ec))
                return std::filesystem::weakly_canonical(b, ec).string();
    }
    // CWD fallback (repo root) — also covers the case where exe dir is unknown
    if (std::filesystem::exists(rel, ec)) return rel;
    return rel; // unchanged: callers use it for a sensible error message
}

} // namespace voc
