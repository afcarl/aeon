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

#include <random>

#include "gtest/gtest.h"
#include "batch_loader_cpio_cache.hpp"

using namespace std;

string randomString() {
    stringstream s;
    std::random_device engine;

    uint x = engine();
    s << x;
    return s.str();
}

class RandomBatchLoader : public BatchLoader {
public:
    RandomBatchLoader(uint block_size) : BatchLoader(block_size) {}

    void loadBlock(buffer_in_array &dest, uint block_num) {
        // load BufferPair with random bytes
        std::random_device engine;

        string object_string = randomString();
        vector<char> obj(object_string.begin(), object_string.end());
        dest[0]->addItem(obj);

        string target = randomString();
        vector<char> tgt(target.begin(), target.end());
        dest[1]->addItem(tgt);
    };

    uint objectCount() {
        return 10;
    }
};

string load_string(BatchLoaderCPIOCache cache) {
    // call loadBlock from cache and cast the resulting item to a uint

    // buffer_in* dataBuffer = new buffer_in(0);
    // buffer_in* targetBuffer = new buffer_in(0);
    buffer_in_array bp(vector<size_t>{0, 0});

    cache.loadBlock(bp, 1);

    vector<char>& x = bp[0]->getItem(0);
    string str(x.data(), x.size());
    return str;
}

BatchLoaderCPIOCache make_cache(const string& rootCacheDir,
                                const string& hash,
                                const string& version) {
    BatchLoaderCPIOCache cache(
        rootCacheDir, hash, version, make_shared<RandomBatchLoader>(1)
    );
    return cache;
}

TEST(batch_loader_cpio_cache, integration) {
    // load the same block twice and make sure it has the same value.
    // RandomBatchLoader always returns a different uint value no matter
    // the block_num.  The only way two consequetive calls are the same
    // is if the cache is working properly

    auto cache = make_cache("/tmp", randomString(), "version123");

    ASSERT_EQ(load_string(cache), load_string(cache));
}

TEST(batch_loader_cpio_cache, same_version) {
    string hash = randomString();
    ASSERT_EQ(
        load_string(make_cache("/tmp", hash, "version123")),
        load_string(make_cache("/tmp", hash, "version123"))
    );
}

TEST(batch_loader_cpio_cache, differnt_version) {
    string hash = randomString();
    ASSERT_NE(
        load_string(make_cache("/tmp", hash, "version123")),
        load_string(make_cache("/tmp", hash, "version456"))
    );
}

TEST(batch_loader_cpio_cache, differnt_hash) {
    ASSERT_NE(
        load_string(make_cache("/tmp", randomString(), "version123")),
        load_string(make_cache("/tmp", randomString(), "version123"))
    );
}
