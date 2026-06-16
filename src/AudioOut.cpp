#include "AudioOut.h"
#include "Wav.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef GRUNT_HAVE_VORBIS
#include <vorbis/vorbisenc.h>
#include <cstdlib>
#include <ctime>
#endif

namespace voc {

const char* format_ext(AudioFormat f) {
    return f == AudioFormat::Wav ? "wav" : "ogg";
}

AudioFormat format_from_string(const std::string& s) {
    std::string t; for (char c : s) t += (char)std::tolower((unsigned char)c);
    if (t == "wav") return AudioFormat::Wav;
    // "ogg", "vorbis", "ogg/vorbis", anything else -> Ogg/Vorbis
    return AudioFormat::Ogg;
}

bool ogg_supported() {
#ifdef GRUNT_HAVE_VORBIS
    return true;
#else
    return false;
#endif
}

#ifdef GRUNT_HAVE_VORBIS
// Encode a mono float buffer to an OGG file carrying a Vorbis stream.
// quality is the VBR quality in [-0.1, 1.0] (libvorbis convention).
static bool write_ogg_vorbis(const std::string& path, const AudioBuffer& buf,
                             float quality, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { err = "cannot open " + path; return false; }

    vorbis_info vi;
    vorbis_info_init(&vi);
    // mono, VBR by quality (NOT Opus — this is the Vorbis encoder)
    if (vorbis_encode_init_vbr(&vi, 1, buf.sample_rate, quality) != 0) {
        vorbis_info_clear(&vi); std::fclose(f);
        err = "vorbis_encode_init_vbr failed"; return false;
    }

    vorbis_comment vc; vorbis_comment_init(&vc);
    vorbis_comment_add_tag(&vc, "ENCODER", "grunt");

    vorbis_dsp_state vd; vorbis_block vb;
    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    ogg_stream_state os;
    std::srand((unsigned)std::time(nullptr));
    ogg_stream_init(&os, std::rand());

    // headers
    {
        ogg_packet hp, hc, hcode;
        vorbis_analysis_headerout(&vd, &vc, &hp, &hc, &hcode);
        ogg_stream_packetin(&os, &hp);
        ogg_stream_packetin(&os, &hc);
        ogg_stream_packetin(&os, &hcode);
        ogg_page og;
        while (ogg_stream_flush(&os, &og)) {
            std::fwrite(og.header, 1, og.header_len, f);
            std::fwrite(og.body, 1, og.body_len, f);
        }
    }

    const long CHUNK = 1024;
    size_t pos = 0;
    bool eos = false;
    while (!eos) {
        long n = (long)std::min<size_t>(CHUNK, buf.samples.size() - pos);
        if (pos >= buf.samples.size()) {
            vorbis_analysis_wrote(&vd, 0); // signal end
        } else {
            float** enc = vorbis_analysis_buffer(&vd, n);
            for (long i = 0; i < n; ++i) enc[0][i] = buf.samples[pos + i];
            vorbis_analysis_wrote(&vd, n);
            pos += n;
        }

        while (vorbis_analysis_blockout(&vd, &vb) == 1) {
            vorbis_analysis(&vb, nullptr);
            vorbis_bitrate_addblock(&vb);
            ogg_packet op;
            while (vorbis_bitrate_flushpacket(&vd, &op)) {
                ogg_stream_packetin(&os, &op);
                ogg_page og;
                while (ogg_stream_pageout(&os, &og)) {
                    std::fwrite(og.header, 1, og.header_len, f);
                    std::fwrite(og.body, 1, og.body_len, f);
                    if (ogg_page_eos(&og)) eos = true;
                }
            }
        }
    }

    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    std::fclose(f);
    return true;
}
#endif // GRUNT_HAVE_VORBIS

bool write_audio(const std::string& path, const AudioBuffer& buf,
                 AudioFormat fmt, float quality, std::string& err) {
    if (fmt == AudioFormat::Wav) return write_wav(path, buf, err);

    // Ogg/Vorbis
#ifdef GRUNT_HAVE_VORBIS
    return write_ogg_vorbis(path, buf, quality, err);
#else
    (void)quality;
    err = "this build was compiled without libvorbis; rebuild with "
          "-DGRUNT_HAVE_VORBIS (and link vorbisenc/vorbis/ogg) or use --format wav";
    return false;
#endif
}

} // namespace voc
