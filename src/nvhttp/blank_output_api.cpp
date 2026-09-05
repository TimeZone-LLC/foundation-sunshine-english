/**
 * @file src/nvhttp/blank_output_api.cpp
 * @brief Definitions for the client-facing blank output toggle endpoint.
 */
#include "blank_output_api.h"

#include <string>

#include <Simple-Web-Server/server_http.hpp>
#include <nlohmann/json.hpp>

#include "src/blank_output.h"
#include "src/logging.h"
#include "src/stream.h"

using json = nlohmann::json;

namespace nvhttp::blank_output_api {

  namespace {

    SimpleWeb::CaseInsensitiveMultimap
    json_headers() {
      SimpleWeb::CaseInsensitiveMultimap headers;
      headers.emplace("Content-Type", "application/json");
      return headers;
    }

    bool
    any_session_running() {
      for (const auto &info : stream::session::get_all_sessions_info()) {
        if (info.state == "RUNNING") {
          return true;
        }
      }
      return false;
    }

    void
    reply(const resp_https_t &response, SimpleWeb::StatusCode status, int status_code, const std::string &message) {
      json body;
      body["status_code"] = status_code;
      body["status_message"] = message;
      body["enabled"] = blank_output::enabled();
      response->write(status, body.dump(), json_headers());
      response->close_connection_after_response = true;
    }

  }  // namespace

  void
  handle(resp_https_t response, req_https_t request) {
    BOOST_LOG(debug) << "Request - Protocol: HTTPS"
                     << ", IP: " << request->remote_endpoint().address().to_string()
                     << ", PORT: " << request->remote_endpoint().port()
                     << ", METHOD: " << request->method
                     << ", PATH: " << request->path;

    try {
      const auto args = request->parse_query_string();
      const auto enabled_param = args.find("enabled");

      if (enabled_param == args.end()) {
        reply(response, SimpleWeb::StatusCode::success_ok, 200, "OK");
        return;
      }

      const auto requested = blank_output::parse_flag(enabled_param->second);
      if (!requested) {
        BOOST_LOG(warning) << "blank_output: invalid enabled value [" << enabled_param->second << "]";
        reply(response, SimpleWeb::StatusCode::client_error_bad_request, 400, "Invalid enabled value. Must be 0 or 1");
        return;
      }

      if (!any_session_running()) {
        BOOST_LOG(warning) << "blank_output: rejected, no active streaming session";
        reply(response, SimpleWeb::StatusCode::client_error_not_found, 404, "No active streaming session");
        return;
      }

      blank_output::set(*requested);
      BOOST_LOG(info) << "blank_output: " << (*requested ? "enabled" : "disabled") << " by client "
                      << request->remote_endpoint().address().to_string();
      reply(response, SimpleWeb::StatusCode::success_ok, 200, "OK");
    }
    catch (const std::exception &e) {
      BOOST_LOG(error) << "blank_output: " << e.what();
      reply(response, SimpleWeb::StatusCode::server_error_internal_server_error, 500, "Internal server error");
    }
  }

}  // namespace nvhttp::blank_output_api
