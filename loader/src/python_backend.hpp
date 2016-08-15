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

#include <map>
#include "interface.hpp"
#include <Python.h>

#include <assert.h>
#include "buffer_in.hpp"
#include "util.hpp"
#include "buffer_out.hpp"

namespace nervana {
    class python_backend;
}

class nervana::python_backend {
public:
    python_backend(PyObject*, const std::vector<nervana::shape_type>&, int);
    ~python_backend();

    bool use_pinned_memory();
    void call_backend_transfer(nervana::buffer_out_array &outBuf, int bufIdx);
    PyObject* get_host_tuple(int bufIdx);
    PyObject* get_shapes();
    const std::vector<nervana::shape_type>& _oshape_types;
    int                         _batchSize;
private:
    python_backend() = delete;
    PyObject* initPyList(int length=2);
    void wrap_buffer_pool(PyObject *list, nervana::buffer_out *buf, int bufIdx,
                          const nervana::shape_type& shape_type);

    PyObject*                   _py_obj_backend;

    std::vector<PyObject*>      _host_lists;
    std::vector<PyObject*>      _dev_lists;

    PyObject*                   _f_consume = NULL;
};
