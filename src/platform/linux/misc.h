/**
 * @file src/platform/linux/misc.h
 * @brief Miscellaneous declarations for Linux.
 */
#pragma once

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <vector>

#include "src/utility.h"

KITTY_USING_MOVE_T(file_t, int, -1, {
  if (el >= 0) {
    close(el);
  }
});

enum class window_system_e {
  NONE,  ///< No window system
  X11,  ///< X11
  WAYLAND,  ///< Wayland
};

extern window_system_e window_system;

#ifdef SUNSHINE_BUILD_PORTAL
namespace portal {
  inline std::jthread xdg_worker;
  inline std::once_flag xdg_worker_flag;

  void clear_saved_token();
  bool has_saved_token();
  bool is_portal_service_reachable();
}  // namespace portal
#endif

namespace dyn {
  typedef void (*apiproc)(void);

  int
  load(void *handle, const std::vector<std::tuple<apiproc *, const char *>> &funcs, bool strict = true);
  void *
  handle(const std::vector<const char *> &libs);

}  // namespace dyn

namespace platf {
  bool sanitize_process_environment();
  bool has_elevated_privileges(bool all_caps);
  void drop_elevated_privileges(bool all_caps);
  int open_drm_card_fd(const std::filesystem::path &path, int flags = O_RDWR);
  inline bool
  get_env(std::string_view name, std::string &value) {
    const std::string key { name };
    if (const char *raw = std::getenv(key.c_str())) {
      value = raw;
      return true;
    }
    value.clear();
    return false;
  }

  inline std::string
  get_env(std::string_view name) {
    std::string value;
    get_env(name, value);
    return value;
  }

  /** Resolve the DRM render node used for Vulkan/VA-API device creation. */
  std::string resolve_render_device();

#if defined(__linux__) && defined(SUNSHINE_BUILD_DRM)
  namespace kms {
    void ensure_privileged_drm_worker_started();
    void drop_drm_worker_privileges();
    int privileged_open_drm_card_fd(const char *path);
  }  // namespace kms
#endif

  /**
   * Advance a capture deadline without accumulating drift, re-anchoring after
   * a discontinuity instead of trying to replay stale frames.
   */
  inline void
  handle_pacing(std::chrono::steady_clock::time_point &next_frame, std::chrono::nanoseconds delay, auto &logger) {
    const auto now = std::chrono::steady_clock::now();
    if (next_frame > now) {
      std::this_thread::sleep_until(next_frame);
      logger.first_point(next_frame);
      logger.second_point_now_and_log();
    }

    next_frame += delay;
    if (next_frame < now) {
      next_frame = now + delay;
    }
  }
}  // namespace platf
