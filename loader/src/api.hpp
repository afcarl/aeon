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

#include "batchfile.hpp"

extern "C" {

static std::string last_error_message;

extern const char* get_error_message() {
    return last_error_message.c_str();
}

extern void* start(int* itemCount, int miniBatchSize,
                   bool shuffleManifest, bool shuffleEveryEpoch,
                   int datumSize, int datumTypeSize,
                   int targetSize, int targetTypeSize,
                   int subsetPercent,
                   const char* mediaConfigString,
                   // MediaParams* mediaParams,
                   DeviceParams* deviceParams,
                   const char* manifestFilename,
                   int macroBatchSize,
                   const char* rootCacheDir,
                   int randomSeed) {
    static_assert(sizeof(int) == 4, "int is not 4 bytes");
    try {
        Loader* loader = new Loader(miniBatchSize,
                                    shuffleManifest, shuffleEveryEpoch,
                                    datumSize, datumTypeSize,
                                    targetSize, targetTypeSize,
                                    subsetPercent,
                                    mediaConfigString, deviceParams,
                                    manifestFilename,
                                    macroBatchSize,
                                    rootCacheDir,
                                    randomSeed);
        int result = loader->start();
        if (result != 0) {
            std::stringstream ss;
            ss << "Could not start data loader. Error " << result;
            last_error_message = ss.str();
            delete loader;
            return 0;
        }
        *itemCount = loader->itemCount();
        return reinterpret_cast<void*>(loader);
    } catch(std::exception& ex) {
        last_error_message = ex.what();
        return 0;
    }
}

extern int error() {
    try {
        throw std::runtime_error("abc error");
    } catch(std::exception& ex) {
        last_error_message = ex.what();
        return -1;
    }
}

extern int next(Loader* loader) {
    try {
        loader->next();
        return 0;
    } catch(std::exception& ex) {
        last_error_message = ex.what();
        return -1;
    }
}

extern int reset(Loader* loader) {
    try {
        return loader->reset();
    } catch(std::exception& ex) {
        last_error_message = ex.what();
        return -1;
    }
}

extern int stop(Loader* loader) {
    try {
        loader->stop();
        delete loader;
        return 0;
    } catch(std::exception& ex) {
        last_error_message = ex.what();
        return -1;
    }
}

extern void write_batch(char *outfile, const int numData,
                        char **jpgfiles, uint32_t *targets,
                        int maxDim) {
#if HAS_IMGLIB
    if (numData == 0) {
        return;
    }
    BatchFileWriter bf;
    bf.open(outfile, "imgclass");
    for (int i=0; i<numData; i++) {
        ByteVect inp;
        readFileBytes(jpgfiles[i], inp);
        if (maxDim != 0) {
            resizeInput(inp, maxDim);
        }
        ByteVect tgt(sizeof(uint32_t));
        memcpy(&tgt[0], &(targets[i]), sizeof(uint32_t));
        bf.writeItem(inp, tgt);
    }
    bf.close();
#else
    std::string message = "OpenCV " UNSUPPORTED_MEDIA_MESSAGE;
    throw std::runtime_error(message);
#endif
}

extern void write_raw(char *outfile, const int numData,
                  char **jpgdata, uint32_t *jpglens,
                  uint32_t *targets) {
    if (numData == 0) {
        return;
    }
    BatchFileWriter bf;
    uint32_t tgtSize = sizeof(uint32_t);
    bf.open(outfile);
    for (int i=0; i<numData; i++) {
        bf.writeItem(jpgdata[i], (char *) &targets[i], jpglens[i], tgtSize);
    }
    bf.close();
}

extern int read_max_item(char *batchfile) {
    BatchFileReader bf;
    if(bf.open(batchfile)) {
        int maxItemSize = bf.maxDatumSize();
        bf.close();
        return maxItemSize;
    } else {
        std::stringstream ss;
        ss << "couldn't open " << batchfile;
        throw std::runtime_error(ss.str());
    }
}

}
