#pragma once
#include <memory>
#include <random>
#include "argtype.hpp"
#include "json.hpp"

namespace nervana {
    class decoded_media;
    class settings;
    class json_parameter_collection;
}

typedef std::shared_ptr<nervana::decoded_media>        media_ptr;
typedef std::shared_ptr<nervana::json_parameter_collection> param_ptr;
typedef std::shared_ptr<nervana::settings>             settings_ptr;

enum class MediaType {
    UNKNOWN = -1,
    IMAGE = 0,
    VIDEO = 1,
    AUDIO = 2,
    TEXT = 3,
    TARGET = 4,
};

/*  ABSTRACT INTERFACES */
class nervana::decoded_media {
public:
    virtual ~decoded_media() {}
    virtual MediaType get_type() = 0;
};

/*  ABSTRACT INTERFACES */
class nervana::settings {
public:
    virtual ~settings() {}
};

class nervana::json_parameter_collection {
public:
    template<typename T> void parse_dist(T& value, const std::string key, const nlohmann::json &js)
    {
        auto val = js.find(key);
        if (val != js.end()) {
            auto params = val->get<std::vector<typename T::result_type>>();
            set_dist_params(value, params);
        }
    }

    template<typename T, typename S> inline void set_dist_params(T& dist, S& params)
    {
        dist = T{params[0], params[1]};
    }

    inline void set_dist_params(std::bernoulli_distribution& dist, std::vector<bool>& params)
    {
        dist = std::bernoulli_distribution{params[0] ? 0.5 : 0.0};
    }

    template<typename T> void parse_value(
                                    T& value,
                                    const std::string key,
                                    const nlohmann::json &js,
                                    bool required=false )
    {
        auto val = js.find(key);
        if (val != js.end()) {
            value = val->get<T>();
        } else if (required) {
            throw std::invalid_argument("Required Argument: " + key + " not set");
        }
    }

    template<typename T> void parse_req(T& value, const std::string key, const nlohmann::json &js)
    {
        parse_value(value, key, js, true);
    }

    template<typename T> void parse_opt(T& value, const std::string key, const nlohmann::json &js)
    {
        parse_value(value, key, js, false);
    }
};
