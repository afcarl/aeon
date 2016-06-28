/*
 Copyright 2015 Nervana Systems Inc.
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#pragma once

#include <time.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <sstream>

#include "buffer.hpp"

#define FORMAT_VERSION  1
#define WRITER_VERSION  1
#define MAGIC_STRING    "MACR"
#define CPIO_FOOTER     "TRAILER!!!"


typedef std::vector<std::string> LineList;
typedef std::vector<char> ByteVect;
typedef std::pair<std::shared_ptr<ByteVect>,std::shared_ptr<ByteVect>> DataPair;

static_assert(sizeof(int) == 4, "int is not 4 bytes");
static_assert(sizeof(uint) == 4, "uint is not 4 bytes");
static_assert(sizeof(short) == 2, "short is not 2 bytes");

/*

The data is stored as a cpio archive and may be unpacked using the
GNU cpio utility.

    https://www.gnu.org/software/cpio/

Individual items are packed into a macrobatch file as follows:
    - header
    - datum 1
    - target 1
    - datum 2
    - target 2
      ...
    - trailer

Each of these items comprises of a cpio header record followed by data.

*/

class RecordHeader {
public:
    RecordHeader();
    void loadDoubleShort(uint* dst, ushort src[2]);

    void saveDoubleShort(ushort* dst, uint src);

    void read(std::istream& ifs, uint* fileSize);

    void write(std::ostream& ofs, uint fileSize, const char* fileName);

public:
    ushort                      _magic;
    ushort                      _dev;
    ushort                      _ino;
    ushort                      _mode;
    ushort                      _uid;
    ushort                      _gid;
    ushort                      _nlink;
    ushort                      _rdev;
    ushort                      _mtime[2];
    ushort                      _namesize;
    ushort                      _filesize[2];
};

class BatchFileHeader {
friend class BatchReader;
friend class BatchFileWriter;
public:
    BatchFileHeader();
    void read(std::istream& ifs);
    void write(std::ostream& ofs);

private:
#pragma pack(1)
    char                        _magic[4];
    uint                        _formatVersion;
    uint                        _writerVersion;
    char                        _dataType[8];
    uint                        _itemCount;
    uint                        _maxDatumSize;
    uint                        _maxTargetSize;
    uint                        _totalDataSize;
    uint                        _totalTargetsSize;
    char                        _unused[24];
#pragma pack()
};

class BatchFileTrailer {
public:
    BatchFileTrailer() ;
    void write(std::ostream& ofs);
    void read(std::istream& ifs);

private:
    uint                        _unused[4];
};

class BatchReader {
public:
    BatchReader();
    BatchReader(std::istream* is);

    void read(Buffer& dest);

    int itemCount() ;

    int totalDataSize() ;

    int totalTargetsSize();

    int maxDatumSize() ;

    int maxTargetSize();

protected:
    void readHeader();

    std::istream*               _is;

    BatchFileHeader             _fileHeader;
    BatchFileTrailer            _fileTrailer;
    RecordHeader                _recordHeader;
    std::string                 _fileName;
    std::string                 _tempName;
};

/*
 * BatchFileReader wraps file opening around the more generic BatchReader
 * which only deals in istreams
 */

class BatchFileReader : public BatchReader {
public:
    BatchFileReader();
    ~BatchFileReader();

    bool open(const std::string& fileName);
    void close();

private:
    std::ifstream               _ifs;
};

class BatchFileWriter {
public:
    ~BatchFileWriter();

    void open(const std::string& fileName, const std::string& dataType = "");
    void close();

    void writeItem(char* datum, char* target,
                   uint datumSize, uint targetSize);

    void writeItem(ByteVect &datum, ByteVect &target);

private:
    std::ofstream               _ofs;
    BatchFileHeader             _fileHeader;
    BatchFileTrailer            _fileTrailer;
    RecordHeader                _recordHeader;
    int                         _fileHeaderOffset;
    std::string                 _fileName;
    std::string                 _tempName;
};
extern int readFileLines(const std::string &filn, LineList &ll);
extern int readFileBytes(const std::string &filn, ByteVect &b);



