/**
 * @file src/platform/permissions.h
 * @brief Cross-platform permission status and request interface.
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace platf {
  /**
   * @brief Access needed by the current Sunshine configuration.
   */
  struct permission_status_t {
    std::string id;  ///< Stable identifier used by the Web UI.
    std::string status;  ///< Granted, denied, not_determined, on_use, or unknown.
    bool required;  ///< Whether the active configuration needs this access.
    bool verifiable;  ///< Whether the OS permits a passive status check.
    bool requestable = false;  ///< Whether Sunshine can initiate a request or settings action.
  };

  /**
   * @brief Query access needed by Sunshine on the current platform.
   */
  std::vector<permission_status_t>
  get_permission_statuses();

  /**
   * @brief Determine whether every verifiable required permission is granted.
   */
  bool
  required_permissions_granted(const std::vector<permission_status_t> &permissions);

  /**
   * @brief Initiate a native permission request or settings action.
   * @param id Stable identifier of the access item.
   * @return True when the action was recognized and initiated.
   */
  bool
  request_permission(std::string_view id);
}  // namespace platf
