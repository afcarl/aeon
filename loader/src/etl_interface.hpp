#pragma once
#include <memory>
#include "params.hpp"

namespace nervana {
    namespace interface {
        class extractor;
        class transformer;
        class loader;
    }
}

class nervana::interface::extractor {
public:
    virtual ~extractor() {}
    virtual media_ptr extract(char*, int) = 0;
};

class nervana::interface::transformer {
public:
    virtual ~transformer() {}
    virtual media_ptr transform(settings_ptr, const media_ptr&) = 0;
    virtual void fill_settings(settings_ptr) = 0;
};

class nervana::interface::loader {
public:
    virtual ~loader() {}
    virtual void load(char*, int, const media_ptr&) = 0;
};
