#pragma once

#include "etl_label.hpp"
#include "etl_image.hpp"
#include "provider.hpp"
#include "etl_localization.hpp"

namespace nervana {
    namespace image {
        class randomizing_provider;
    }
    namespace label {
        class int_provider;
    }
    namespace localization {
        class default_provider;
    }

    class image::randomizing_provider : public provider<image::decoded, image::params> {
    public:
        randomizing_provider(std::shared_ptr<image::config> cfg)
        {
            _extractor   = std::make_shared<image::extractor>(cfg);
            _transformer = std::make_shared<image::transformer>(cfg);
            _loader      = std::make_shared<image::loader>(cfg);
            _factory     = std::make_shared<image::param_factory>(cfg);
        }
        ~randomizing_provider() {}
    };

    class label::int_provider : public provider<label::decoded, nervana::params> {
    public:
        int_provider(std::shared_ptr<label::config> cfg)
        {
            _extractor   = std::make_shared<label::extractor>(cfg);
            _transformer = std::make_shared<label::transformer>();
            _loader      = std::make_shared<label::loader>(cfg);
            _factory     = nullptr;
        }
        ~int_provider() {}
    };

    class localization::default_provider : public provider<localization::decoded, image::params> {
    public:
        default_provider(nlohmann::json js)
        {
            auto cfg     = std::make_shared<localization::config>();
            cfg->set_config(js);
            _extractor   = std::make_shared<localization::extractor>(cfg);
            _transformer = std::make_shared<localization::transformer>(cfg);
            _loader      = std::make_shared<localization::loader>(cfg);
            _factory     = nullptr;
        }
    };

    class image_decoder : public train_provider<image::randomizing_provider, label::int_provider> {
    public:
        image_decoder(nlohmann::json js)
        {
            auto data_config = std::make_shared<image::config>();
            data_config->set_config(js["data_config"]["config"]);   // TODO: check return value
            _dprov = std::make_shared<image::randomizing_provider>(data_config);

            auto target_config = std::make_shared<label::config>();
            target_config->set_config(js["target_config"]["config"]);
            _tprov = std::make_shared<label::int_provider>(target_config);
        }
    };

    class localization_decoder : public train_provider<image::randomizing_provider, localization::default_provider> {
    public:
        localization_decoder(nlohmann::json js)
        {
            _dprov = std::make_shared<image::randomizing_provider>(js["data_config"]);
            _tprov = std::make_shared<localization::default_provider>(js["target_config"]);
        }
    };
}
