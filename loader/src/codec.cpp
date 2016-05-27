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

#include <cstring>

#include "codec.hpp"

Codec::Codec(MediaParams* params) : _format(0), _codec(0) {
    if (params->_mtype == VIDEO) {
        _mediaType = AVMEDIA_TYPE_VIDEO;
    } else if (params->_mtype == AUDIO) {
        _mediaType = AVMEDIA_TYPE_AUDIO;
    }
}

RawMedia* Codec::decode(char* item, int itemSize) {
    _format = avformat_alloc_context();
    if (_format == 0) {
        throw std::runtime_error("Could not get context for decoding");
    }
    uchar* itemCopy = (uchar*) av_malloc(itemSize);
    if (itemCopy == 0) {
        throw std::runtime_error("Could not allocate memory");
    }

    memcpy(itemCopy, item, itemSize);
    _format->pb = avio_alloc_context(itemCopy, itemSize, 0, 0, 0, 0, 0);

    if (avformat_open_input(&_format , "", 0, 0) < 0) {
        throw std::runtime_error("Could not open input for decoding");
    }

    if (avformat_find_stream_info(_format, 0) < 0) {
        throw std::runtime_error("Could not find media information");
    }

    _codec = _format->streams[0]->codec;
    int stream = av_find_best_stream(_format, _mediaType, -1, -1, 0, 0);
    if (stream < 0) {
        throw std::runtime_error("Could not find media stream in input");
    }

    int result =
        avcodec_open2(_codec, avcodec_find_decoder(_codec->codec_id), 0);
    if (result < 0) {
        throw std::runtime_error("Could not open decoder");
    }

    if (_raw.size() == 0) {
        _raw.addBufs(_codec->channels, itemSize);
    } else {
        _raw.reset();
    }

    _raw.setSampleSize(av_get_bytes_per_sample(_codec->sample_fmt));
    assert(_raw.sampleSize() >= 0);
    AVPacket packet;
    while (av_read_frame(_format, &packet) >= 0) {
        decodeFrame(&packet, stream, itemSize);
    }

    avcodec_close(_codec);
    av_free(_format->pb->buffer);
    av_free(_format->pb);
    avformat_close_input(&_format);
    return &_raw;
}

void Codec::decodeFrame(AVPacket* packet, int stream, int itemSize) {
    int frameFinished;
    if (packet->stream_index == stream) {
        AVFrame* frame = av_frame_alloc();
        int result = 0;
        if (_mediaType == AVMEDIA_TYPE_AUDIO) {
            result = avcodec_decode_audio4(_codec, frame,
                                           &frameFinished, packet);
        } else {
            throw std::runtime_error("Unsupported media");
        }

        if (result < 0) {
            throw std::runtime_error("Could not decode media stream");
        }

        if (frameFinished == true) {
            int frameSize = frame->nb_samples * _raw.sampleSize();
            if (_raw.bufSize() < _raw.dataSize() + frameSize) {
                _raw.growBufs(itemSize);
            }
            _raw.fillBufs((char**) frame->data, frameSize);
        }
        av_frame_free(&frame);
    }
    av_free_packet(packet);
}
