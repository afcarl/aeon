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

#include <assert.h>
#include <vector>
#include <cstring>
#include <initializer_list>

#if HAS_GPU
#include <cuda.h>
#endif

class buffer_out {
public:
    explicit buffer_out(int sizeof_type, int item_count, int minibatch_size, bool pinned = false);
    virtual ~buffer_out();

    char* getItem(int index, int& len);
    char* data() { return _data; }

    int getItemCount();
    uint getSize();

private:
    buffer_out() = delete;
    char* alloc();
    void dealloc(char* data);

    char*   _data;
    size_t  _size;

    bool    _pinned;
    size_t  _stride;
    size_t  _item_size;
};

class buffer_out_array {
public:
    buffer_out_array(std::initializer_list<buffer_out*> list) : data(list) {}

    buffer_out* operator[](int i) { return data[i]; }
    size_t size() const { return data.size(); }

private:
    std::vector<buffer_out*>    data;
};
