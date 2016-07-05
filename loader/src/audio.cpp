#include "audio.hpp"

Audio::Audio(AudioParams *params, int randomSeed)
: _params(params), _noiseClips(0), _loadedNoise(false) {
    _rng.state = 0;
    _codec = new Codec(params);
    _specgram = new Specgram(params, randomSeed);
    if (params->_noiseIndexFile != 0) {
        if ((randomSeed == 0) && (params->_noiseClips == 0)) {
            params->_noiseClips = new NoiseClips(params->_noiseIndexFile, params->_noiseDir, _codec);
            _loadedNoise = true;
        }
        _noiseClips = reinterpret_cast<NoiseClips*>(params->_noiseClips);
        assert(_noiseClips != 0);
    }
}

Audio::~Audio() {
    if (_loadedNoise == true) {
        delete _noiseClips;
    }
    delete _specgram;
    delete _codec;
}

void Audio::dumpToBin(char* filename, RawMedia* audio, int idx) {
    // dump audio file in `audio` at buffer index `idx` into bin
    // file at `filename`.  Assumes buffer is already scaled to
    // int16.  A simple python script can convert this file into
    // a wav file: loader/test/raw_to_wav.py
    FILE *file = fopen(filename, "wb");
    fwrite(audio->getBuf(idx), audio->numSamples(), 2, file);
    fclose(file);
}

void Audio::transform(char* item, int itemSize, char* buf, int bufSize, int* meta) {
    auto raw = decode(item, itemSize);
    newTransform(raw, buf, bufSize, meta);
}

RawMedia* Audio::decode(char* item, int itemSize) {
    return _codec->decode(item, itemSize);
}

void Audio::newTransform(RawMedia* raw, char* buf, int bufSize, int* meta) {
    if (_noiseClips != 0) {
        _noiseClips->addNoise(raw, _rng);
    }
    int len = _specgram->generate(raw, buf, bufSize);
    if (meta != 0) {
        *meta = len;
    }
}

void Audio::ingest(char** dataBuf, int* dataBufLen, int* dataLen) {
}
