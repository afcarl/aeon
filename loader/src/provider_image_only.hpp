#pragma once

#include "provider_interface.hpp"
#include "etl_image.hpp"

namespace nervana {
    class image_only : public provider_interface {
    public:
        image_only(nlohmann::json js);
        void provide(int idx, buffer_in_array& in_buf, buffer_out_array& out_buf);

    private:
        image::config               image_config;
        image::extractor            image_extractor;
        image::transformer          image_transformer;
        image::loader               image_loader;
        image::param_factory        image_factory;
    };
}
