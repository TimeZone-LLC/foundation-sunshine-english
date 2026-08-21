/**
 * @file src/httpcommon.h
 * @brief Declarations for common HTTP.
 */
#pragma once

// standard includes
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>

// lib includes
#include <boost/system/error_code.hpp>
#include <curl/curl.h>
#include <map>
#include <string>

// local includes
#include "network.h"
#include "thread_safe.h"

namespace http {

  /**
   * @brief Process exit codes Sunshine reports to whatever supervises it.
   * @details Exit code 0 means "a stop was requested, do not escalate". Every startup
   *          failure has to report something else, otherwise the service wrapper keeps
   *          respawning Sunshine into the exact same failure forever.
   */
  namespace exit_code {
    constexpr int SUCCESS = 0;  ///< Clean shutdown, requested by the user or the service.
    constexpr int CONFIG_UNUSABLE = 10;  ///< The configuration file or its directory could not be read or written.
    constexpr int PORT_UNAVAILABLE = 11;  ///< A listening port stayed unavailable for the whole retry window.
    constexpr int CREDENTIALS_UNUSABLE = 12;  ///< The TLS credentials could not be created or loaded.
  }  // namespace exit_code

  /**
   * @brief The listening sockets whose bring-up is reported at startup.
   */
  enum class listener_e : std::size_t {
    confighttp,  ///< Web UI HTTPS server.
    nvhttp,  ///< GameStream HTTP server.
    nvhttps,  ///< GameStream HTTPS server.
    rtsp,  ///< RTSP setup server.
    count  ///< Number of tracked listeners. Not a listener itself.
  };

  /**
   * @brief The result of driving a listener through http::start_with_bind_retry().
   */
  enum class bind_outcome_e {
    bound,  ///< The port was bound. A blocking listener has since stopped normally.
    aborted,  ///< Sunshine started shutting down before the port could be bound.
    failed  ///< The port could not be bound. Sunshine has been told to exit with a non-zero code.
  };

  /**
   * @brief A single bring-up attempt for one listener.
   * @details Returns an empty error code once the port is bound - a blocking listener such
   *          as Simple-Web-Server keeps serving until it is stopped and only then returns -
   *          or the error that prevented the bind.
   */
  using bind_attempt_fn = std::function<boost::system::error_code()>;

  /**
   * @brief Record that a listener is accepting connections on a port.
   * @param listener The listener that came up.
   * @param port The TCP port it is listening on.
   */
  void
  listener_ready(listener_e listener, std::uint16_t port);

  /**
   * @brief Record that a listener will never come up.
   * @details Used for failures that happen before a bind is even attempted, such as TLS
   *          credentials that cannot be loaded, so the startup summary still names them.
   * @param listener The listener that failed.
   * @param port The TCP port it would have listened on.
   */
  void
  listener_failed(listener_e listener, std::uint16_t port);

  /**
   * @brief Bring a listener up, retrying a transient bind failure with escalating backoff.
   * @details A port still held by a previous sunshine.exe that is on its way out frees up by
   *          itself, so such attempts are repeated with a growing delay over a bounded window.
   *          A permanent error - a bad bind address, a denied privilege - fails immediately.
   *          When the window closes without a bind, the listener is marked failed and Sunshine
   *          is asked to exit with http::exit_code::PORT_UNAVAILABLE.
   * @param listener The listener being started, named in the log lines and the startup summary.
   * @param port The TCP port the listener binds.
   * @param attempt Performs one bring-up attempt.
   * @return What became of the listener.
   */
  bind_outcome_e
  start_with_bind_retry(listener_e listener, std::uint16_t port, const bind_attempt_fn &attempt);

  /**
   * @brief Wait for every listener to report, then log one English startup summary.
   * @param timeout How long to wait before reporting the listeners that are still silent.
   * @return `true` when every listener bound its port.
   */
  bool
  report_listener_readiness(std::chrono::milliseconds timeout);

  int
  init();
  int
  create_creds(const std::string &pkey, const std::string &cert);
  int
  save_user_creds(
    const std::string &file,
    const std::string &username,
    const std::string &password,
    bool run_our_mouth = false);

  int reload_user_creds(const std::string &file);
  bool download_file(const std::string &url, const std::string &file, long ssl_version = CURL_SSLVERSION_TLSv1_2);
  bool fetch_url(const std::string &url, std::string &content, long ssl_version = CURL_SSLVERSION_TLSv1_2);
  bool get_json(const std::string &url, const std::map<std::string, std::string> &headers, std::string &response_body, long &http_code, long timeout_seconds = 30);
  bool post_json(const std::string &url, const std::string &body, const std::map<std::string, std::string> &headers, std::string &response_body, long &http_code, long timeout_seconds = 120);
  bool download_image_with_magic_check(const std::string &url, const std::string &file, long ssl_version = CURL_SSLVERSION_TLSv1_2);
  bool download_public_cover_image(const std::string &url, const std::string &file, long ssl_version = CURL_SSLVERSION_TLSv1_2);
  std::string url_escape(const std::string &url);
  std::string url_get_host(const std::string &url);

  extern std::string unique_id;
  extern net::net_e origin_web_ui_allowed;

}  // namespace http
