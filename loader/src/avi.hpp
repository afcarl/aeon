#pragma once

#include <deque>
#include <string>
#include <fstream>
#include <sstream>
#include <cinttypes>

#include "util.hpp"

namespace nervana {
    class MjpegInputStream;
    class MjpegFileInputStream;
    class MjpegMemoryInputStream;


#ifndef DWORD
typedef uint32_t DWORD;
#endif
#ifndef WORD
typedef uint16_t WORD;
#endif
#ifndef LONG
typedef int32_t  LONG;
#endif

#pragma pack(push, 1)
struct AviMainHeader
{
    DWORD dwMicroSecPerFrame;    //  The period between video frames
    DWORD dwMaxBytesPerSec;      //  Maximum data rate of the file
    DWORD dwReserved1;           // 0
    DWORD dwFlags;               //  0x10 AVIF_HASINDEX: The AVI file has an idx1 chunk containing an index at the end of the file.
    DWORD dwTotalFrames;         // Field of the main header specifies the total number of frames of data in file.
    DWORD dwInitialFrames;       // Is used for interleaved files
    DWORD dwStreams;             // Specifies the number of streams in the file.
    DWORD dwSuggestedBufferSize; // Field specifies the suggested buffer size forreading the file
    DWORD dwWidth;               // Fields specify the width of the AVIfile in pixels.
    DWORD dwHeight;              // Fields specify the height of the AVIfile in pixels.
    DWORD dwReserved[4];         // 0, 0, 0, 0
};

struct AviStreamHeader
{
    uint32_t fccType;              // 'vids', 'auds', 'txts'...
    uint32_t fccHandler;           // "cvid", "DIB "
    DWORD dwFlags;               // 0
    DWORD dwPriority;            // 0
    DWORD dwInitialFrames;       // 0
    DWORD dwScale;               // 1
    DWORD dwRate;                // Fps (dwRate - frame rate for video streams)
    DWORD dwStart;               // 0
    DWORD dwLength;              // Frames number (playing time of AVI file as defined by scale and rate)
    DWORD dwSuggestedBufferSize; // For reading the stream
    DWORD dwQuality;             // -1 (encoding quality. If set to -1, drivers use the default quality value)
    DWORD dwSampleSize;          // 0 means that each frame is in its own chunk
    struct {
        short int left;
        short int top;
        short int right;
        short int bottom;
    } rcFrame;                // If stream has a different size than dwWidth*dwHeight(unused)
};

struct AviIndex
{
    DWORD ckid;
    DWORD dwFlags;
    DWORD dwChunkOffset;
    DWORD dwChunkLength;
};

struct BitmapInfoHeader
{
    DWORD biSize;                // Write header size of BITMAPINFO header structure
    LONG  biWidth;               // width in pixels
    LONG  biHeight;              // heigth in pixels
    WORD  biPlanes;              // Number of color planes in which the data is stored
    WORD  biBitCount;            // Number of bits per pixel
    DWORD biCompression;         // Type of compression used (uncompressed: NO_COMPRESSION=0)
    DWORD biSizeImage;           // Image Buffer. Quicktime needs 3 bytes also for 8-bit png
                                 //   (biCompression==NO_COMPRESSION)?0:xDim*yDim*bytesPerPixel;
    LONG  biXPelsPerMeter;       // Horizontal resolution in pixels per meter
    LONG  biYPelsPerMeter;       // Vertical resolution in pixels per meter
    DWORD biClrUsed;             // 256 (color table size; for 8-bit only)
    DWORD biClrImportant;        // Specifies that the first x colors of the color table. Are important to the DIB.
};

struct RiffChunk
{
    uint32_t m_four_cc;
    uint32_t m_size;
};

struct RiffList
{
    uint32_t m_riff_or_list_cc;
    uint32_t m_size;
    uint32_t m_list_type_cc;
};

#pragma pack(pop)

}


typedef std::deque< std::pair<uint64_t, uint32_t> > frame_list;
typedef frame_list::iterator frame_iterator;

class nervana::MjpegInputStream
{
public:
    MjpegInputStream(){}
    virtual ~MjpegInputStream(){}
    virtual MjpegInputStream& read(char*, uint64_t) = 0;
    virtual MjpegInputStream& seekg(uint64_t) = 0;
    virtual uint64_t tellg() = 0;
    virtual bool isOpened() const = 0;
    virtual bool open(const std::string& filename) = 0;
    virtual void close() = 0;
    virtual operator bool() = 0;
};

class nervana::MjpegFileInputStream : public nervana::MjpegInputStream
{
public:
    MjpegFileInputStream();
    MjpegFileInputStream(const std::string& filename);
    ~MjpegFileInputStream();
    MjpegInputStream& read(char*, uint64_t) override;
    MjpegInputStream& seekg(uint64_t) override;
    uint64_t tellg() override;
    bool isOpened() const override;
    bool open(const std::string& filename) override;
    void close() override;
    operator bool() override;

private:
    bool            m_is_valid;
    std::ifstream   m_f;
};

class nervana::MjpegMemoryInputStream : public nervana::MjpegInputStream
{
public:
    MjpegMemoryInputStream();
    MjpegMemoryInputStream(char* data, size_t size);
    ~MjpegMemoryInputStream();
    MjpegInputStream& read(char*, uint64_t) override;
    MjpegInputStream& seekg(uint64_t) override;
    uint64_t tellg() override;
    bool isOpened() const override;
    bool open(const std::string& filename) override;
    void close() override;
    operator bool() override;

private:
    bool                        m_is_valid;
    nervana::memstream<char>    m_wrapper;
    std::istream                m_f;
};

nervana::MjpegInputStream& operator >> (nervana::MjpegInputStream& is, nervana::AviMainHeader& avih);
nervana::MjpegInputStream& operator >> (nervana::MjpegInputStream& is, nervana::AviStreamHeader& strh);
nervana::MjpegInputStream& operator >> (nervana::MjpegInputStream& is, nervana::BitmapInfoHeader& bmph);
nervana::MjpegInputStream& operator >> (nervana::MjpegInputStream& is, nervana::RiffList& riff_list);
nervana::MjpegInputStream& operator >> (nervana::MjpegInputStream& is, nervana::RiffChunk& riff_chunk);
nervana::MjpegInputStream& operator >> (nervana::MjpegInputStream& is, nervana::AviIndex& idx1);