#include "wav_data.hpp"

using namespace std;
using nervana::pack_le;
using nervana::unpack_le;

namespace nervana {

    wav_data::wav_data(char *buf, uint32_t bufsize)
    {
        char *bptr = buf;

        wav_assert(bufsize >= HEADER_SIZE, "Header size is too small");

        RiffMainHeader rh;
        FmtHeader fh;

        memcpy(&rh, bptr, sizeof(rh)); bptr += sizeof(rh);
        memcpy(&fh, bptr, sizeof(fh)); bptr += sizeof(fh);

        wav_assert(rh.dwRiffCC == nervana::FOURCC('R', 'I', 'F', 'F'), "Unsupported format");
        wav_assert(rh.dwWaveID == nervana::FOURCC('W', 'A', 'V', 'E'), "Unsupported format");
        wav_assert(bufsize >= rh.dwRiffLen, "Buffer not large enough for indicated file size");

        wav_assert(fh.hwFmtTag == WAVE_FORMAT_PCM, "can read only PCM data");
        wav_assert(fh.hwChannels == 16, "Ingested waveforms must be 16-bit PCM");
        wav_assert(fh.dwFmtLen >= 16, "PCM format data must be at least 16 bytes");

        // Skip any subchunks between "fmt" and "data".
        while (strncmp(bptr, "data", 4) != 0) {
            uint32_t chunk_sz = unpack_le<uint32_t>(bptr + 4);
            wav_assert(chunk_sz == 4 || strncmp(bptr, "fact", 4), "Malformed fact chunk");
            bptr += 4 + sizeof(chunk_sz) + chunk_sz; // chunk tag, chunk size, chunk
        }

        wav_assert(strncmp(bptr, "data", 4) == 0, "Expected data tag not found");

        DataHeader dh;
        memcpy(&dh, bptr, sizeof(dh)); bptr += sizeof(dh);

        uint32_t num_samples = dh.dwDataLen / fh.hwBlockAlign;
        data.create(num_samples, fh.hwChannels, CV_16SC1);
        _sample_rate = fh.dwSampleRate;

        for (uint32_t n = 0; n < data.rows; ++n) {
            for (uint32_t c = 0; c < data.cols; ++c) {
                data.at<int16_t>(n, c) = unpack_le<int16_t>(bptr);
                bptr += sizeof(int16_t);
            }
        }
    }

    void wav_data::dump(std::ostream & ostr)
    {
        ostr << "sample_rate " << _sample_rate << "\n";
        ostr << "channels x samples " << data.size() << "\n";
        ostr << "bit_depth " << (data.elemSize() * 8) << "\n";
        ostr << "nbytes " << nbytes() << "\n";
    }

    void wav_data::write_to_file(string filename)
    {
        uint32_t totsize = HEADER_SIZE + nbytes();
        char* buf = new char[totsize];

        write_to_buffer(buf, totsize);

        std::ofstream ofs;
        ofs.open(filename, ostream::binary);
        wav_assert(ofs.is_open(), "couldn't open file for writing: " + filename);
        ofs.write(buf, totsize);
        ofs.close();
        delete[] buf;
    }

    void wav_data::write_to_buffer(char *buf, uint32_t bufsize)
    {
        uint32_t reqsize = nbytes() + HEADER_SIZE;
        wav_assert(bufsize >= reqsize,
                   "output buffer is too small " + to_string(bufsize) + " vs " + to_string(reqsize));
        write_header(buf, HEADER_SIZE);
        write_data(buf + HEADER_SIZE, nbytes());
    }

    void wav_data::write_header(char *buf, uint32_t bufsize)
    {
        RiffMainHeader rh;
        rh.dwRiffCC      = nervana::FOURCC('R', 'I', 'F', 'F');
        rh.dwRiffLen     = HEADER_SIZE + nbytes();
        rh.dwWaveID      = nervana::FOURCC('W', 'A', 'V', 'E');

        FmtHeader fh;
        fh.dwFmtCC       = nervana::FOURCC('f', 'm', 't', ' ');
        fh.dwFmtLen      = sizeof(FmtHeader) - 2 * sizeof(uint32_t);
        fh.hwFmtTag      = WAVE_FORMAT_PCM;
        fh.hwChannels    =  data.cols;
        fh.dwSampleRate  = _sample_rate;
        fh.dwBytesPerSec = _sample_rate * data.elemSize();
        fh.hwBlockAlign  = data.elemSize() * data.cols;
        fh.hwBitDepth    = data.elemSize() * 8;

        DataHeader dh;
        dh.dwDataCC      = nervana::FOURCC('d', 'a', 't', 'a');
        dh.dwDataLen     = nbytes();

        // buf.resize();
        assert(bufsize >= HEADER_SIZE);

        memcpy(buf, &rh, sizeof(rh)); buf += sizeof(rh);
        memcpy(buf, &fh, sizeof(fh)); buf += sizeof(fh);
        memcpy(buf, &dh, sizeof(dh));

    }

    void wav_data::write_data(char* buf, uint32_t bufsize)
    {
        assert(bufsize >= nbytes());
        for (int n = 0; n < data.rows; n++) {
            int16_t *ptr = data.ptr<int16_t>(n);
            for (int c = 0; c < data.cols; c++) {
                pack_le(buf, ptr[c], (n * data.cols + c) * sizeof(int16_t));
            }
        }
    }
}
