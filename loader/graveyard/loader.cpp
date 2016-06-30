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

#include <assert.h>

#include <vector>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <utility>
#include <algorithm>

#include "matrix.hpp"
#include "device.hpp"
#include "loader.hpp"
#include "batch_loader_cpio_cache.hpp"
#include "sequential_batch_iterator.hpp"
#include "shuffled_batch_iterator.hpp"

using namespace std;

DecodeThreadPool::DecodeThreadPool(int count,
                                   int batchSize,
                                   nlohmann::json configJs,
                                   DeviceParams *dp)
: ThreadPool(count),
  _itemsPerThread((batchSize - 1) / count + 1),
  _endSignaled(0),
  _manager(0), _stopManager(false), _managerStopped(false), _inputBuf(0),
  _bufferIndex(0), _batchSize(batchSize), _deviceParams(dp)
{
    assert(_itemsPerThread * count >= _batchSize);
    assert(_itemsPerThread * (count - 1) < _batchSize);
    for (int i = 0; i < count; i++) {
        auto prov = nervana::train_provider_factory::create(configJs);
        _providers.push_back(prov);
        _startSignaled.push_back(0);
        _startInds.push_back(0);
        _endInds.push_back(0);
        _dataOffsets.push_back(0);
        _targetOffsets.push_back(0);
    }

    _deviceParams->_batchSize = _batchSize;

    _providers[0]->fill_dtm_load_info(&(_deviceParams->_dtmInfo));
    _providers[0]->fill_tgt_load_info(&(_deviceParams->_tgtInfo));

    _datumLen  = _deviceParams->_dtmInfo.size * _deviceParams->_dtmInfo.count;
    _targetLen = _deviceParams->_tgtInfo.size * _deviceParams->_tgtInfo.count;
}

DecodeThreadPool::~DecodeThreadPool()
{
    if (_manager != 0) {
        _manager->join();
        delete _manager;
    }
    // The other thread objects are freed in the destructor
    // of the parent class.
}

void DecodeThreadPool::set_io_buffers(const std::shared_ptr<BufferPool>& in,
                                      const std::shared_ptr<Device>& device,
                                      const std::shared_ptr<BufferPool>& out)
{
    _in = in;
    _device = device;
    _out = out;
}

void DecodeThreadPool::start()
{
    for (int i = 0; i < _count; i++) {
        _threads.push_back(new thread(&DecodeThreadPool::run, this, i));
    }
    _manager = new thread(&DecodeThreadPool::manage, this);
}

void DecodeThreadPool::stop()
{
    ThreadPool::stop();
    while (stopped() == false) {
        std::this_thread::yield();
        _in->advanceWritePos();
        _in->signalNonEmpty();
    }

    _stopManager = true;
    while (_managerStopped == false) {
        std::this_thread::yield();
        _in->advanceWritePos();
        _in->signalNonEmpty();
        _endSignaled++;
        _ended.notify_one();
    }
}

void DecodeThreadPool::run(int id)
{
    // Initialize worker threads by computing memory offsets for the
    // data this thread should work on
    assert(id < _count);
    _startInds[id] = id * _itemsPerThread;
    int itemCount = _itemsPerThread;
    if (id == _count - 1) {
        itemCount = _batchSize - id * _itemsPerThread;
    }

    _endInds[id] = _startInds[id] + itemCount;
    _dataOffsets[id] = _startInds[id] * _datumLen;
    _targetOffsets[id] = _startInds[id] * _targetLen;
    while (_done == false) {
        work(id);
    }

    _stopped[id] = true;
}

void DecodeThreadPool::work(int id)
{
    // Thread function.
    {
        unique_lock<mutex> lock(_mutex);
        while (_startSignaled[id] == 0) {
            _started.wait(lock);
            if (_done == true) {
                return;
            }
        }
        _startSignaled[id]--;
        assert(_startSignaled[id] == 0);
    }

    int start = _startInds[id];
    int end = _endInds[id];
    // No locking required because threads
    // write into non-overlapping regions.
    BufferPair& outBuf = _out->getForWrite();
    char* dataBuf = outBuf.first->_data + _dataOffsets[id];
    char* targetBuf = outBuf.second->_data + _targetOffsets[id];
    for (int i = start; i < end; i++) {
        _providers[id]->provide_pair(i, _inputBuf, dataBuf, targetBuf);
        dataBuf   += _datumLen;
        targetBuf += _targetLen;
    }

    {
        lock_guard<mutex> lock(_mutex);
        _endSignaled++;
        assert(_endSignaled <= _count);
    }
    _ended.notify_one();
}

void DecodeThreadPool::produce()
{
    // Produce a minibatch.
    {
        unique_lock<mutex> lock(_out->getMutex());
        while (_out->full() == true) {
            _out->waitForNonFull(lock);
        }
        {
            lock_guard<mutex> lock(_mutex);
            for (unsigned int i = 0; i < _startSignaled.size(); i++) {
                _startSignaled[i] = 1;
            }
        }
        _started.notify_all();
        {
            unique_lock<mutex> lock(_mutex);
            while (_endSignaled < _count) {
                _ended.wait(lock);
            }
            _endSignaled = 0;
        }
        // At this point, we have decoded data for the whole minibatch.
        BufferPair& outBuf = _out->getForWrite();
        Matrix::transpose(outBuf.first->_data, _batchSize,
                          _deviceParams->_dtmInfo.count, _deviceParams->_dtmInfo.size);
        Matrix::transpose(outBuf.second->_data, _batchSize,
                          _deviceParams->_tgtInfo.count, _deviceParams->_tgtInfo.size);
        // Copy to device.
        _device->copyData(_bufferIndex, outBuf.first->_data, outBuf.first->_size);
        _device->copyLabels(_bufferIndex, outBuf.second->_data, outBuf.second->_size);
        _bufferIndex = (_bufferIndex == 0) ? 1 : 0;
        _out->advanceWritePos();
    }
    _out->signalNonEmpty();
}

void DecodeThreadPool::consume()
{
    // Consume an input buffer.
    {
        unique_lock<mutex> lock(_in->getMutex());
        while (_in->empty() == true) {
            _in->waitForNonEmpty(lock);
            if (_stopManager == true) {
                return;
            }
        }
        _inputBuf = &_in->getForRead();
        produce();
        _in->advanceReadPos();
    }
    _in->signalNonFull();
}

void DecodeThreadPool::manage()
{
    // Thread function.
    int result = _device->init();
    if (result != 0) {
        _stopManager = true;
    }
    while (_stopManager == false) {
        consume();
    }
    _managerStopped = true;
}

ReadThread::ReadThread(const shared_ptr<BufferPool>& out,
                       const shared_ptr<BatchIterator>& batch_iterator)
: ThreadPool(1), _out(out), _batch_iterator(batch_iterator)
{
    assert(_count == 1);
}

void ReadThread::work(int id)
{
    // Fill input buffers.
    {
        unique_lock<mutex> lock(_out->getMutex());
        while (_out->full() == true) {
            _out->waitForNonFull(lock);
        }
        _batch_iterator->read(_out->getForWrite());
        _out->advanceWritePos();
    }
    _out->signalNonEmpty();
}

Loader::Loader(int miniBatchSize, const char* loaderConfigString, DeviceParams *deviceParams)
: _first(true),
  _miniBatchSize(miniBatchSize),
  _deviceParams(deviceParams),
  _readBufs(nullptr), _decodeBufs(nullptr), _readThread(nullptr), _decodeThreads(nullptr),
  _device(nullptr), _batch_iterator(nullptr)
{
    _loaderConfigJson = nlohmann::json::parse(loaderConfigString);

    LoaderConfig loaderConfig;
    loaderConfig.set_config(_loaderConfigJson);

    // the manifest defines which data should be included in the dataset
    _manifest = make_shared<Manifest>(loaderConfig.manifest_filename,
                                      loaderConfig.shuffle_manifest,
                                      loaderConfig.random_seed);

    auto batchFileLoader = make_shared<BatchFileLoader>(_manifest,
                                                        loaderConfig.subset_percent);

    auto batchCacheLoader = make_shared<BatchLoaderCPIOCache>(loaderConfig.cache_directory,
                                                              _manifest->hash(),
                                                              _manifest->version(),
                                                              batchFileLoader);
    if (loaderConfig.shuffle_every_epoch) {
        _batch_iterator = make_shared<ShuffledBatchIterator>(batchCacheLoader,
                                                             loaderConfig.macrobatch_size,
                                                             loaderConfig.random_seed);
    } else {
        _batch_iterator = make_shared<SequentialBatchIterator>(batchCacheLoader,
                                                               loaderConfig.macrobatch_size);
    }
}

int Loader::start()
{
    _first = true;
    try {
        int numCores = thread::hardware_concurrency();
        int itemsPerThread = (_miniBatchSize - 1) /  numCores + 1;
        int threadCount =  (_miniBatchSize - 1) / itemsPerThread + 1;
        threadCount = std::min(threadCount, _miniBatchSize);

        // Create the decode threads first, which interpret the config string to know output sizes
        _decodeThreads = unique_ptr<DecodeThreadPool>(
            new DecodeThreadPool(threadCount, _miniBatchSize, _loaderConfigJson, _deviceParams)
        );

        int dataLen   = _decodeThreads->get_dtm_len() * _miniBatchSize;
        int targetLen = _decodeThreads->get_tgt_len() * _miniBatchSize;

        // Start the read buffers off with a reasonable size. They will get resized as needed.
        _readBufs = make_shared<BufferPool>(dataLen / 8, targetLen);
        _readThread = unique_ptr<ReadThread>(new ReadThread(_readBufs, _batch_iterator));

        // Do the allocation in here, set the pointers in _deviceParams
        _device = Device::create(_deviceParams, true);

        bool pinned = (_device->_type != CPU);
        _decodeBufs = make_shared<BufferPool>(dataLen, targetLen, pinned);

        _decodeThreads->set_io_buffers(_readBufs, _device, _decodeBufs);

    } catch(std::bad_alloc&) {
        return -1;
    }
    _decodeThreads->start();
    _readThread->start();
    return 0;
}

void Loader::stop()
{
    _readThread->stop();
    while (_readThread->stopped() == false) {
        std::this_thread::yield();
        drain();
    }
    while ((_decodeBufs->empty() == false) ||
           (_readBufs->empty() == false)) {
        drain();
    }
    _decodeThreads->stop();

    _readBufs = nullptr;
    _readThread = nullptr;
    _decodeBufs = nullptr;
    _decodeThreads = nullptr;
}

int Loader::reset()
{
    stop();
    _batch_iterator->reset();
    start();
    return 0;
}

void Loader::next()
{
    unique_lock<mutex> lock(_decodeBufs->getMutex());
    if (_first == true) {
        _first = false;
    } else {
        // Unlock the buffer used for the previous minibatch.
        _decodeBufs->advanceReadPos();
        _decodeBufs->signalNonFull();
    }
    while (_decodeBufs->empty()) {
        _decodeBufs->waitForNonEmpty(lock);
    }
}

void Loader::drain()
{
    {
        unique_lock<mutex> lock(_decodeBufs->getMutex());
        if (_decodeBufs->empty() == true) {
            return;
        }
        _decodeBufs->advanceReadPos();
    }
    _decodeBufs->signalNonFull();
}
