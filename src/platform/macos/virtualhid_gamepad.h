/**
 * @file src/platform/macos/virtualhid_gamepad.h
 * @brief Platform virtual gamepad support backed by libvirtualhid.
 */
#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <libvirtualhid/libvirtualhid.hpp>

#include "src/platform/common.h"

namespace platf::virtualhid {

  /**
   * @brief Owns the libvirtualhid runtime and its virtual gamepads.
   *
   * Keyboard and mouse injection deliberately remain in the existing
   * CoreGraphics backend. This adapter supplies only the gamepad capability
   * that macOS was missing.
   */
  class virtual_gamepad_backend_t {
  public:
    struct gamepad_context_t;

    explicit virtual_gamepad_backend_t(lvh::BackendKind backend = lvh::BackendKind::platform_default);
    ~virtual_gamepad_backend_t();

    virtual_gamepad_backend_t(const virtual_gamepad_backend_t &) = delete;
    virtual_gamepad_backend_t &operator=(const virtual_gamepad_backend_t &) = delete;

    int allocate(
      const gamepad_id_t &id,
      const gamepad_arrival_t &metadata,
      feedback_queue_t feedback_queue,
      std::string_view requested_profile
    );
    void free(int nr);
    void update(int nr, const gamepad_state_t &state);
    void touch(const gamepad_touch_t &touch);
    void motion(const gamepad_motion_t &motion);
    void battery(const gamepad_battery_t &battery);

    [[nodiscard]] bool has_gamepad(int nr) const;
    [[nodiscard]] bool is_dualsense(int nr) const;
    [[nodiscard]] lvh::Runtime *runtime() const;

#ifdef SUNSHINE_TESTS
    [[nodiscard]] lvh::GamepadStateAdapter *adapter_for_testing(int nr) const;
#endif

  private:
    std::unique_ptr<lvh::Runtime> runtime_;
    std::vector<std::shared_ptr<gamepad_context_t>> gamepads_;
  };

  /**
   * @brief Profiles accepted before the platform runtime has been initialized.
   */
  [[nodiscard]] std::vector<supported_gamepad_t> static_supported_gamepads();

  /**
   * @brief Profiles currently usable through the installed platform broker.
   */
  [[nodiscard]] std::vector<supported_gamepad_t> supported_gamepads(lvh::Runtime *runtime);

  /**
   * @brief Whether a licensed platform broker can currently create gamepads.
   */
  [[nodiscard]] bool runtime_gamepads_available(lvh::Runtime *runtime);

  /**
   * @brief Whether the configured profile can use Moonlight controller extensions.
   */
  [[nodiscard]] bool profile_supports_controller_touch(std::string_view profile);

}  // namespace platf::virtualhid
