#pragma once

#include <iostream>
#include <memory>
#include <chrono>
#include <random>
#include <mutex>
#include <tuple>

#include <cpprest/http_listener.h>

#include "json.hpp"
#include "loader.hpp"
#include "log.hpp"

namespace nervana
{
    template <typename T>
    class statused_response
    {
    public:
        statused_response(int _status_code, const T& _value)
            : status_code(_status_code)
            , value(_value)
        {
        }
        int status_code;
        T   value;
    };

    using json_response = statused_response<web::json::value>;
    using next_tuple    = std::tuple<web::json::value, std::string>;

    class loader_adapter
    {
    public:
        loader_adapter(const nlohmann::json& config)
            : m_loader(config)
        {
        }
        void        reset();
        std::string next();

        std::string batch_size() const;
        std::string names_and_shapes() const;
        std::string batch_count() const;
        std::string record_count() const;

    private:
        nervana::loader_local m_loader;
        std::mutex            m_mutex;
        bool                  m_is_reset{ true };
    };

    class loader_manager
    {
    public:
        loader_manager()
            : m_id_generator(std::random_device{}())
        {
        }
        uint32_t register_agent(const nlohmann::json& config);
        void                     unregister_agent(uint32_t);
        nervana::loader_adapter& loader(uint32_t id);

    private:
        const uint32_t max_loader_number = 1000;
        std::mt19937   m_id_generator;
        std::mutex     m_mutex;
        std::map<uint32_t, std::unique_ptr<nervana::loader_adapter>> m_loaders;
    };

    class server_parser
    {
    public:
        server_parser();
        json_response post(std::string msg, std::string msg_body);
        json_response del(std::string msg);
        statused_response<next_tuple> get(std::string msg);

    private:
        const std::string version         = "v1";
        const std::string endpoint_prefix = "/" + version + "/dataset";
        typedef web::json::value (server_parser::*msg_process_func_t)(loader_adapter&);
        std::map<std::string, server_parser::msg_process_func_t> process_func;

        loader_manager m_loader_manager;

        statused_response<next_tuple> next(loader_adapter& loader);
        web::json::value batch_size(loader_adapter& loader);
        web::json::value reset(loader_adapter& loader);
        web::json::value names_and_shapes(loader_adapter& loader);
        web::json::value batch_count(loader_adapter& loader);
        web::json::value record_count(loader_adapter& loader);
    };

  class daemon {
  public:
    explicit daemon(const std::string& addr)
      : listener_{ web::http::uri_builder{ addr }.append_path(U("api")).to_uri() }
    {
      listener_.support(web::http::methods::POST,
                        std::bind(&daemon::handle_post, this, std::placeholders::_1));
      listener_.support(web::http::methods::GET,
                        std::bind(&daemon::handle_get, this, std::placeholders::_1));
      listener_.support(web::http::methods::DEL,
                        std::bind(&daemon::handle_delete, this, std::placeholders::_1));

      listener_.open().wait();
    }

    daemon() = delete;

    daemon(const daemon&) = delete;
    daemon& operator = (const daemon&) = delete;

    daemon(daemon&&) = delete;
    daemon& operator = (daemon&&) = delete;

    ~daemon() {
      listener_.close().wait();
    }

  private:
    web::http::experimental::listener::http_listener listener_;
    server_parser server_parser_;

    void handle_post(web::http::http_request r) {
      log::info("POST %s", r.relative_uri().path());
      r.reply(web::http::status_codes::Accepted,
              server_parser_.post(web::uri::decode(r.relative_uri().path()),
                                  r.extract_string().get()));
    }
    void handle_get(web::http::http_request r) {
      log::info("GET %s", r.relative_uri().path());
      auto answer{ server_parser_.get(web::uri::decode(r.relative_uri().path())) };
      r.reply(std::get<1>(answer).empty() ?
              web::http::status_codes::OK : web::http::status_codes::Accepted,
              std::get<0>(answer));
    }
    void handle_delete(web::http::http_request r) {
      log::info("DELETE %s", r.relative_uri().path());
      r.reply(web::http::status_codes::OK,
              server_parser_.del(web::uri::decode(r.relative_uri().path())));
    }
  };
}
