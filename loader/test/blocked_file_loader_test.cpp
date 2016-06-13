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

#include "gtest/gtest.h"
#include "blocked_file_loader.hpp"
#include "manifest_maker.hpp"

using namespace std;

TEST(blocked_file_loader, constructor) {
    Manifest m("manifest.txt");
    BlockedFileLoader bfl(&m);
}

TEST(blocked_file_loader, loadBlock) {
    uint block_size = 2;
    uint object_size = 16;
    uint target_size = 16;

    Manifest m(tmp_manifest_file(4, object_size, target_size));

    BlockedFileLoader bfl(&m);

    // TODO: move this initialization which is copied from buffer.cpp
    // into constructor and destructor of BufferPair
    Buffer* dataBuffer = new Buffer(0);
    Buffer* targetBuffer = new Buffer(0);
    BufferPair bp = make_pair(dataBuffer, targetBuffer);

    bfl.loadBlock(bp, 0, block_size);

    uint* object_data = (uint*)bp.first->_data;
    uint* target_data = (uint*)bp.second->_data;

    // the object_data and target_data should be full of repeating
    // uints.  the uints in target_data will be 1 bigger than the uints
    // in object_data.  Make sure that this is the case here.
    for(uint i = 0; i < object_size / sizeof(uint) * block_size; ++i) {
        ASSERT_EQ(object_data[i] + 1, target_data[i]);
    }

    delete bp.first;
    delete bp.second;
}
