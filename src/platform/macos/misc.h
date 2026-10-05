/**
 * @file src/platform/macos/misc.h
 * @brief Miscellaneous declarations for macOS platform.
 */
#pragma once

#include <vector>

#include <CoreGraphics/CoreGraphics.h>

#include "src/platform/permissions.h"

namespace platf {
  /**
   * @brief Action to take when a macOS privacy permission is unavailable.
   */
  enum class permission_request_action_t {
    none,
    prompt,
    settings,
  };

  /**
   * @brief Choose a privacy action without repeating an earlier startup prompt.
   */
  permission_request_action_t
  permission_request_action(bool granted, int previous_state, bool startup, bool retry_manual_request);

  /**
   * @brief Check whether this macOS major version has Local Network privacy controls.
   */
  bool
  supports_local_network_privacy(int major_version);

  /**
   * @brief Decide whether startup should initiate a native permission request.
   */
  bool
  should_request_startup_permission(const permission_status_t &permission);

  /**
   * @brief Request missing permissions needed by the active configuration.
   */
  void
  request_startup_permissions();
}  // namespace platf

namespace dyn {
  typedef void (*apiproc)();

  int
  load(void *handle, const std::vector<std::tuple<apiproc *, const char *>> &funcs, bool strict = true);
  void *
  handle(const std::vector<const char *> &libs);

}  // namespace dyn
