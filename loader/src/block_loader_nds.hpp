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

#pragma once

#include <sstream>
#include <string>

#include "buffer_in.hpp"
#include "cpio.hpp"
#include "block_loader.hpp"

namespace nervana {
    class block_loader_nds;
}

class nervana::block_loader_nds : public block_loader {
public:
    block_loader_nds(const std::string& baseurl, const std::string& token, int collection_id, uint32_t block_size, int shard_count=1, int shard_index=0);
    ~block_loader_nds();

    void loadBlock(nervana::buffer_in_array& dest, uint32_t block_num);
    uint32_t objectCount();

    uint32_t blockCount();

private:
    void loadMetadata();

    void get(const std::string& url, std::stringstream& stream);

    const std::string loadBlockURL(uint32_t block_num);
    const std::string metadataURL();

    const std::string _baseurl;
    const std::string _token;
    const int _collection_id;
    const int _shard_count;
    const int _shard_index;
    unsigned int _objectCount;
    unsigned int _blockCount;

    // reuse connection across requests
    void* _curl;
};
