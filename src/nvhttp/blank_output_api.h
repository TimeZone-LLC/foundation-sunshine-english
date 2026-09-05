/**
 * @file src/nvhttp/blank_output_api.h
 * @brief Declarations for the client-facing blank output toggle endpoint.
 */
#pragma once

#include <memory>

#include "src/nvhttp.h"

namespace nvhttp::blank_output_api {

  using resp_https_t = std::shared_ptr<typename SimpleWeb::ServerBase<SunshineHTTPS>::Response>;
  using req_https_t = std::shared_ptr<typename SimpleWeb::ServerBase<SunshineHTTPS>::Request>;

  /**
   * @brief `GET /blank-output[?enabled=0|1]`.
   * @details Without `enabled` the current state is returned. With it, the host stops
   *          capturing and every session sends a static black picture (or resumes) while the
   *          stream stays connected. Only accepted while at least one session is running, so
   *          a stale request can never leave the next stream blanked.
   */
  void
  handle(resp_https_t response, req_https_t request);

}  // namespace nvhttp::blank_output_api
