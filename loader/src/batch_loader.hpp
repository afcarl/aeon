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

#include "buffer.hpp"

/*
 * A BatchLoader is something which can load blocks of data into a
 * BufferPair
 */

class BatchLoader {
public:
    // TODO: this interface could instead take block_size in the
    // constructor, and then either implement block_num as an iterator
    // or at least provide a blockCount function
    virtual void loadBlock(BufferPair& dest, uint block_num, uint block_size) = 0;
    virtual uint objectCount() = 0;
};
