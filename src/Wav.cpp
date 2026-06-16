#include "Wav.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace voc {

double AudioBuffer::peak_dbfs() const {
    float peak = 0.f;
    for (float s : samples) peak = std::max(peak, std::fabs(s));
    if (peak <= 0.f) return -120.0;
    return 20.0 * std::log10((double)peak);
}

namespace {
void put_u32(std::ofstream& o, uint32_t v) { o.write(reinterpret_cast<char*>(&v), 4); }
void put_u16(std::ofstream& o, uint16_t v) { o.write(reinterpret_cast<char*>(&v), 2); }
uint32_t get_u32(const unsigned char* p) { return p[0] | (p[1]<<8) | (p[2]<<16) | ((uint32_t)p[3]<<24); }
uint16_t get_u16(const unsigned char* p) { return p[0] | (p[1]<<8); }
}

bool write_wav(const std::string& path, const AudioBuffer& buf, std::string& err) {
    std::ofstream o(path, std::ios::binary);
    if (!o) { err = "cannot open " + path; return false; }

    const uint16_t channels = 1, bits = 16;
    const uint32_t sr = (uint32_t)buf.sample_rate;
    const uint32_t byte_rate = sr * channels * bits / 8;
    const uint16_t block_align = channels * bits / 8;
    const uint32_t data_bytes = (uint32_t)buf.samples.size() * 2;

    o.write("RIFF", 4); put_u32(o, 36 + data_bytes); o.write("WAVE", 4);
    o.write("fmt ", 4); put_u32(o, 16); put_u16(o, 1); put_u16(o, channels);
    put_u32(o, sr); put_u32(o, byte_rate); put_u16(o, block_align); put_u16(o, bits);
    o.write("data", 4); put_u32(o, data_bytes);

    for (float s : buf.samples) {
        s = std::max(-1.0f, std::min(1.0f, s));
        int16_t v = (int16_t)std::lround(s * 32767.0f);
        o.write(reinterpret_cast<char*>(&v), 2);
    }
    return (bool)o;
}

bool read_wav(const std::string& path, AudioBuffer& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open " + path; return false; }
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), {});
    if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) != 0) { err = "not a WAV: " + path; return false; }

    uint16_t channels = 1, bits = 16; uint32_t sr = 22050;
    size_t pos = 12;
    const unsigned char* data = nullptr; uint32_t data_len = 0;
    while (pos + 8 <= b.size()) {
        const unsigned char* p = b.data() + pos;
        uint32_t csz = get_u32(p + 4);
        if (std::memcmp(p, "fmt ", 4) == 0) {
            if (pos + 8 + 16 <= b.size()) { // need 16 bytes of fmt body
                channels = get_u16(p + 10); sr = get_u32(p + 12); bits = get_u16(p + 22);
            }
        } else if (std::memcmp(p, "data", 4) == 0) {
            data = p + 8; data_len = csz; break;
        }
        size_t advance = (size_t)8 + csz + (csz & 1);
        if (advance == 0) break;       // malformed: avoid infinite loop
        pos += advance;
    }
    if (!data) { err = "no data chunk: " + path; return false; }
    if (bits != 16) { err = "only 16-bit PCM supported: " + path; return false; }

    // Trust nothing from the header: clamp the claimed data length to the bytes
    // actually present after the data chunk. A truncated/garbage WAV can claim a
    // huge data_len, which would otherwise read past the buffer and crash.
    size_t avail = b.size() - (size_t)(data - b.data());
    if (data_len > avail) data_len = (uint32_t)avail;

    out.sample_rate = (int)sr;
    size_t n = data_len / 2;
    out.samples.resize(n);
    for (size_t i = 0; i < n; ++i) {
        int16_t v = (int16_t)get_u16(data + i * 2);
        out.samples[i] = v / 32768.0f;
    }
    // downmix if stereo slipped in
    if (channels == 2) {
        std::vector<float> mono(out.samples.size() / 2);
        for (size_t i = 0; i < mono.size(); ++i)
            mono[i] = 0.5f * (out.samples[2*i] + out.samples[2*i+1]);
        out.samples = std::move(mono);
    }
    return true;
}

} // namespace voc
