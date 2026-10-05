/**
 * @file src/platform/permissions.cpp
 * @brief Shared permission policy.
 */
#include "permissions.h"

#include <algorithm>

namespace platf {
  bool
  required_permissions_granted(const std::vector<permission_status_t> &permissions) {
    return std::ranges::all_of(permissions, [](const auto &permission) {
      return !permission.required || !permission.verifiable || permission.status == "granted";
    });
  }

#ifndef __APPLE__
  std::vector<permission_status_t>
  get_permission_statuses() {
    return {};
  }

  bool
  request_permission(std::string_view id) {
    (void) id;
    return false;
  }
#endif
}  // namespace platf
