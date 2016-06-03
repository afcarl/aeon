#pragma once
#include <random>
#include "etl_interface.hpp"
#include "params.hpp"

using namespace std;

namespace nervana {

    namespace label {
        class params;
        class decoded;

        class extractor;
        class transformer;
        class loader;

        class settings;
    }
}

class nervana::label::settings : public nervana::settings {
public:
    int scale;
    int shift;
    settings() {}
};


class nervana::label::params : public nervana::parameter_collection {
public:
    int ex_offset;

    int tx_scale;
    int tx_shift;

    float ld_offset;
    bool ld_dofloat;

    params() {
        // Optionals with some standard defaults
        ADD_OPTIONAL(ex_offset, "offset to add on extract", "eo", "extract_offset", 0, -100, 100);
        ADD_OPTIONAL(tx_scale, "scale to multiply by on transform", "tsc", "transform_scale", 1, 1, 10);
        ADD_OPTIONAL(tx_shift, "shift to multiply by on transform", "tsh", "transform_shift", 0, 0, 200);
        ADD_OPTIONAL(ld_offset, "offset to add on load if loading as float", "lo", "load_offset", 0.0, -0.9, 0.9);
        ADD_OPTIONAL(ld_dofloat, "load as a float?", "lf", "load_dofloat", false);
    }

};


class nervana::label::decoded : public nervana::decoded_media {
public:
    decoded(int index) :
        _index{index} {}
    virtual ~decoded() override {}

    inline MediaType get_type() override { return MediaType::TARGET; }
    inline int get_index() { return _index; }

private:
    decoded() = delete;
    int _index;
};


class nervana::label::extractor : public nervana::interface::extractor {
public:
    extractor(param_ptr pptr) {
        _ex_offset = static_pointer_cast<nervana::label::params>(pptr)->ex_offset;
    }

    ~extractor() {}

    media_ptr extract(char* buf, int bufSize) override {
        if (bufSize != 4) {
            throw runtime_error("Only 4 byte buffers can be loaded as int32");
        }
        return make_shared<nervana::label::decoded>(*reinterpret_cast<int *>(buf) + _ex_offset);
    }

private:
    int _ex_offset;
};


class nervana::label::transformer : public nervana::interface::transformer {
public:
    transformer(param_ptr pptr) {}

    ~transformer() {}

    media_ptr transform(settings_ptr tx, const media_ptr& mp) override {
        int old_index = static_pointer_cast<nervana::label::decoded>(mp)->get_index();
        shared_ptr<nervana::label::settings> txs = static_pointer_cast<nervana::label::settings>(tx);

        return make_shared<nervana::label::decoded>( old_index * txs->scale + txs->shift );
    }

    // Filling settings is done by the relevant params
    void fill_settings(settings_ptr tx) override {}

};


class nervana::label::loader : public nervana::interface::loader {
public:
    loader(param_ptr pptr) {
        shared_ptr<nervana::label::params> lbl_ptr = static_pointer_cast<nervana::label::params>(pptr);
        _ld_offset = lbl_ptr->ld_offset;
        _ld_dofloat = lbl_ptr->ld_dofloat;
    }
    ~loader() {}

    void load(char* buf, int bufSize, const media_ptr& mp) override {
        int index = static_pointer_cast<nervana::label::decoded>(mp)->get_index();
        if (_ld_dofloat) {
            float ld_index = index + _ld_offset;
            memcpy(buf, &ld_index, bufSize);
        } else {
            memcpy(buf, &index, bufSize);
        }

    }

private:
    float _ld_offset;
    bool _ld_dofloat;
};
