/*
 Copyright 2016 Nervana Systems Inc.
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

#include "loader.hpp"
#include "gen_image.hpp"
#include "gtest/gtest.h"
#include "simple_loader.hpp"

extern gen_image image_dataset;

// Code for unit testing.
using namespace std;

unsigned int sum(char* data, unsigned int len) {
    unsigned int result = 0;
    for (unsigned int i = 0; i < len; i++) {
        result += data[i];
    }
    return result;
}

void single(simple_loader* loader, int epochCount, int minibatchCount,
           int batchSize, int datumSize, int targetSize, unsigned int& sm) {
    BatchIterator* reader = loader;
//    string configString = R"({"media":"image","config":{}})";
//    auto js = nlohmann::json::parse(configString);
    nlohmann::json js = {{"media","image"},{"data_config",{{"height",128},{"width",128}}},{"target_config",{}}};
    cout << js.dump(4) << endl;
    shared_ptr<nervana::train_base> media = nervana::train_provider_factory::create(js);
    cout << __FILE__ << " " << __LINE__ << endl;
    ASSERT_NE(nullptr, media.get());
    unique_ptr<char> dataBuf = unique_ptr<char>(new char[datumSize]);
    memset(dataBuf.get(), 0, datumSize);
//    Buffer dataBuffer(0);
//    Buffer targetBuffer(0);
//    BufferPair bufPair = make_pair(&dataBuffer, &targetBuffer);
//    for (int epoch = 0; epoch < epochCount; epoch++) {
//        reader->reset();
//        for (int i = 0; i < minibatchCount; i++) {
//            bufPair.first->reset();
//            bufPair.second->reset();
//            reader->read(bufPair);
//            for (int j = 0; j < batchSize; j++) {
////                int itemSize = 0;
////                char* item = bufPair.first->getItem(j, itemSize);
////                assert(item != 0);
////                media->transform(item, itemSize, dataBuf.get(), datumSize);
////                sm += sum(dataBuf.get(), datumSize);
////                int targetChunkSize = 0;
////                char* targets = bufPair.second->getItem(j, targetChunkSize);
////                sm += sum(targets, targetSize);
//            }
//        }
//    }
}

void multi(simple_loader* loader, int epochCount, int minibatchCount,
          int batchSize, int datumSize, int targetSize, unsigned int& sm) {
//    int result = loader->start();
//    assert(result == 0);
//    int dataSize = batchSize * datumSize;
//    int targetsSize = batchSize * targetSize;
//    char* data = new char[dataSize];
//    char* targets = new char[targetsSize];
//    memset(data, 0, dataSize);
//    memset(targets, 0, targetsSize);
//    shared_ptr<Device> device = loader->getDevice();
//    for (int epoch = 0; epoch < epochCount; epoch++) {
//        loader->reset();
//        for (int i = 0; i < minibatchCount; i++) {
////            loader->next();
////            int bufIdx = i % 2;
////            device->copyDataBack(bufIdx, data, dataSize);
////            device->copyLabelsBack(bufIdx, targets, targetsSize);
////            sm += sum(data, dataSize);
////            sm += sum(targets, targetsSize);
//        }
//    }
//    loader->stop();
//    delete[] data;
//    delete[] targets;
}

int test(const char* repoDir, const char* indexFile, int batchSize,
         int nchan, int height, int width)
{
    int datumSize = nchan * height * width;
    int targetSize = 1;
    int datumTypeSize = 1;
    int targetTypeSize = 4;
    int epochCount = 2;
    int minibatchCount = 65;
    int datumLen = datumSize * datumTypeSize;
    int targetLen = targetSize * targetTypeSize;

    string mediaConfigString = R"(
        {
            "height" : 100,
            "width"  : 100
        }
    )";

    char* dataBuffer[2];
    char* targetBuffer[2];
    for (int i = 0; i < 2; i++) {
        dataBuffer[i] = new char[batchSize * datumLen];
        targetBuffer[i] = new char[batchSize * targetLen];
    }

    string archiveDir(repoDir);
    //archiveDir += "-ingested";
    CpuParams deviceParams(0, 0, dataBuffer, targetBuffer);
//    simple_loader loader(batchSize,
//                  false, false, datumSize, datumTypeSize,
//                  targetSize, targetTypeSize, 100,
//                  mediaConfigString.c_str(), &deviceParams, "", 128, "", 0);
    simple_loader loader(repoDir);
    unsigned int singleSum = 0;
    single(&loader, epochCount, minibatchCount, batchSize, datumLen, targetLen, singleSum);
    unsigned int multiSum = 0;
    multi(&loader, epochCount, minibatchCount, batchSize, datumLen, targetLen, multiSum);
    for (int i = 0; i < 2; i++) {
        delete[] dataBuffer[i];
        delete[] targetBuffer[i];
    }
    // printf("sum %u true sum %u\n", multiSum, singleSum);
    EXPECT_EQ( multiSum, singleSum );
    // assert(multiSum == singleSum);
    // printf("OK\n");
    return 0;
}

 TEST(thread,loader) {
     int nchan = 3;
     int height = 128;
     int width = 128;
     int batchSize = 16;
     const char* repoDir = image_dataset.GetDatasetPath().c_str();
     const char* indexFile = "";

     test(repoDir, indexFile, batchSize, nchan, height, width);
 }
