#/*
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

#include <cassert>

#include "device.hpp"

using namespace std;

shared_ptr<Device> Device::create(DeviceParams* params, bool alloc) {
#if HAS_GPU
    if (params->_type == CPU) {
        return make_shared<Cpu>(reinterpret_cast<CpuParams*>(params), alloc);
    }
    return make_shared<Gpu>(reinterpret_cast<GpuParams*>(params), alloc);
#else
    assert(params->_type == CPU);
    return make_shared<Cpu>(reinterpret_cast<CpuParams*>(params), alloc);
#endif
}
