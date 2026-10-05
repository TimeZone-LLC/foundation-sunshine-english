/**
 * @file src/platform/macos/gamepad_mode.h
 * @brief Pure mapping from process-level per-app gamepad modes to profile names.
 */
#pragma once

#include <string_view>

namespace platf::macos {
  using namespace std::literals;

  /**
   * @brief Convert the stable process.cpp gamepad mode IDs to config profile names.
   * @return An empty view for inherit (0) and invalid mode IDs.
   */
  constexpr std::string_view
  gamepad_preference_from_process_mode(const int mode) noexcept {
    switch (mode) {
      case 1:
        return "auto"sv;
      case 2:
        return "x360"sv;
      case 3:
        return "ds4"sv;
      case 4:
        return "ds5"sv;
      case 5:
        return "generic"sv;
      case 6:
        return "xone"sv;
      case 7:
        return "xseries"sv;
      case 8:
        return "switch"sv;
      case 0:
      default:
        return {};
    }
  }
}  // namespace platf::macos
