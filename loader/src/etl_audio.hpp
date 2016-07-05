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

#include "etl_interface.hpp"
#include "params.hpp"

namespace nervana {
    namespace audio {
        class config;
        class params;
        class decoded;

        // goes from config -> params
        class param_factory;

        class extractor;
        class transformer;
        class loader;
    }

    class audio::params : public nervana::params {
        friend class audio::param_factory;
    public:

        void dump(std::ostream & = std::cout);
    private:
        params() {}
    };

    class audio::param_factory : public interface::param_factory<audio::decoded, audio::params> {
    public:
        param_factory(std::shared_ptr<audio::config> cfg,
                      std::default_random_engine& dre) : _cfg{cfg}, _dre{dre} {}
        ~param_factory() {}

        std::shared_ptr<audio::params> make_params(std::shared_ptr<const decoded>);
    private:
        std::shared_ptr<audio::config> _cfg;
        std::default_random_engine& _dre;
    };

    class audio::config : public json_config_parser {
    public:
        bool set_config(nlohmann::json js) override;
    };

    class audio::decoded : public decoded_media {
    public:
        decoded() {}
    };

    class audio::extractor : public interface::extractor<audio::decoded> {
    public:
        extractor(std::shared_ptr<const audio::config>);
        ~extractor() {}
        virtual std::shared_ptr<audio::decoded> extract(const char*, int) override;
    };


    class audio::transformer : public interface::transformer<audio::decoded, audio::params> {
    public:
        transformer(std::shared_ptr<const audio::config>);
        ~transformer() {}
        virtual std::shared_ptr<audio::decoded> transform(
                                                std::shared_ptr<audio::params>,
                                                std::shared_ptr<audio::decoded>) override;
    };
}
