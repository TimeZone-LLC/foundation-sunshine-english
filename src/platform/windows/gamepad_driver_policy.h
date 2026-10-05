/**
 * @file src/platform/windows/gamepad_driver_policy.h
 * @brief Pure Windows virtual-gamepad backend routing policy.
 */
#pragma once

#include <string_view>

namespace platf::windows::gamepad_driver {

  enum class policy_e {
    vigembus,
    virtualhid,
    all,
  };

  constexpr policy_e parse(std::string_view value) {
    if (value == "virtualhid") {
      return policy_e::virtualhid;
    }
    if (value == "all") {
      return policy_e::all;
    }
    return policy_e::vigembus;
  }

  constexpr bool prefers_virtualhid(policy_e policy) {
    return policy != policy_e::vigembus;
  }

  constexpr bool eagerly_initializes_existing_backend(
    policy_e policy,
    bool virtualhid_available
  ) {
    return policy != policy_e::virtualhid || !virtualhid_available;
  }

  // Both opt-in policies retain the established Sunshine backends as a
  // recovery path. `all` documents that intent; `virtualhid` still falls back
  // rather than dropping an active controller when the broker is unavailable.
  constexpr bool allows_existing_fallback(policy_e /*policy*/) {
    return true;
  }

  constexpr bool virtualhid_supports_profile(std::string_view profile) {
    return profile == "auto" || profile == "generic" || profile == "x360" ||
           profile == "xone" || profile == "xseries" || profile == "ds4" ||
           profile == "ds5" || profile == "switch";
  }

  constexpr bool vigembus_supports_profile(std::string_view profile) {
    return profile == "auto" || profile == "x360" || profile == "ds4";
  }

  constexpr bool existing_backend_supports_controller_touch(std::string_view profile) {
    return profile == "auto" || profile == "ds4" || profile == "ds5";
  }

}  // namespace platf::windows::gamepad_driver
