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

#include "block_loader.hpp"
#include "block_iterator.hpp"

namespace nervana {
    class block_iterator_sequential;
}

class nervana::block_iterator_sequential : public block_iterator {
public:
    block_iterator_sequential(std::shared_ptr<block_loader> loader);
    void read(nervana::buffer_in_array& dest);
    void reset();

private:
    std::shared_ptr<block_loader> _loader;
    uint _count;
    uint _i;
};
