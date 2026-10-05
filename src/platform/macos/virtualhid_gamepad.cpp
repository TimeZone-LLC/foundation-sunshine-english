/**
 * @file src/platform/macos/virtualhid_gamepad.cpp
 * @brief Platform virtual gamepad support backed by libvirtualhid.
 */

#include "virtualhid_gamepad.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <utility>

#include <libvirtualhid/license.hpp>

#include "src/config.h"
#include "src/logging.h"

using namespace std::literals;

namespace platf::virtualhid {

  struct virtual_gamepad_backend_t::gamepad_context_t {
    std::unique_ptr<lvh::GamepadStateAdapter> adapter;
    feedback_queue_t feedback_queue;
    std::mutex feedback_mutex;
    std::array<std::optional<std::uint32_t>, 2> touch_ids;
    std::uint8_t client_relative_index = 0;
    lvh::GamepadProfileKind profile_kind = lvh::GamepadProfileKind::generic;

    bool has_last_rumble = false;
    std::uint16_t last_low_frequency_rumble = 0;
    std::uint16_t last_high_frequency_rumble = 0;
    bool has_last_trigger_rumble = false;
    std::uint16_t last_left_trigger_rumble = 0;
    std::uint16_t last_right_trigger_rumble = 0;
    bool has_last_rgb = false;
    std::uint8_t last_red = 0;
    std::uint8_t last_green = 0;
    std::uint8_t last_blue = 0;
  };

  namespace {

    struct gamepad_profile_t {
      std::string_view name;
      lvh::GamepadProfileKind kind;
      lvh::DeviceProfile (*profile)();
    };

    constexpr std::array gamepad_profiles {
      gamepad_profile_t {"generic", lvh::GamepadProfileKind::generic, lvh::profiles::generic_gamepad},
      gamepad_profile_t {"x360", lvh::GamepadProfileKind::xbox_360, lvh::profiles::xbox_360},
      gamepad_profile_t {"xone", lvh::GamepadProfileKind::xbox_one, lvh::profiles::xbox_one},
      gamepad_profile_t {"xseries", lvh::GamepadProfileKind::xbox_series, lvh::profiles::xbox_series},
      gamepad_profile_t {"ds4", lvh::GamepadProfileKind::dualshock4, lvh::profiles::dualshock4},
      gamepad_profile_t {"ds5", lvh::GamepadProfileKind::dualsense, lvh::profiles::dualsense},
      gamepad_profile_t {"switch", lvh::GamepadProfileKind::switch_pro, lvh::profiles::switch_pro},
    };

    void log_failure(std::string_view operation, const lvh::OperationStatus &status) {
      if (!status.ok()) {
        BOOST_LOG(warning) << operation << ": "sv << status.message();
      }
    }

    const gamepad_profile_t *find_profile(std::string_view name) {
      const auto iter = std::ranges::find(gamepad_profiles, name, &gamepad_profile_t::name);
      return iter == gamepad_profiles.end() ? nullptr : std::addressof(*iter);
    }

    const gamepad_profile_t &automatic_profile(const gamepad_arrival_t &metadata) {
      if (metadata.type == LI_CTYPE_PS) {
        BOOST_LOG(info) << "Gamepad will be DualSense controller (auto-selected by client-reported type)"sv;
        return *find_profile("ds5"sv);
      }
      if (metadata.type == LI_CTYPE_NINTENDO) {
        BOOST_LOG(info) << "Gamepad will be Nintendo Switch Pro controller (auto-selected by client-reported type)"sv;
        return *find_profile("switch"sv);
      }
      if (metadata.type == LI_CTYPE_XBOX) {
        BOOST_LOG(info) << "Gamepad will be Xbox Series controller (auto-selected by client-reported type)"sv;
        return *find_profile("xseries"sv);
      }
      if (config::input.motion_as_ds4 && (metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO))) {
        BOOST_LOG(info) << "Gamepad will be DualSense controller (auto-selected by motion sensor presence)"sv;
        return *find_profile("ds5"sv);
      }
      if (config::input.touchpad_as_ds4 && (metadata.capabilities & LI_CCAP_TOUCHPAD)) {
        BOOST_LOG(info) << "Gamepad will be DualSense controller (auto-selected by touchpad presence)"sv;
        return *find_profile("ds5"sv);
      }

      BOOST_LOG(info) << "Gamepad will be Xbox Series controller (default)"sv;
      return *find_profile("xseries"sv);
    }

    const gamepad_profile_t &select_profile(
      std::string_view requested_profile,
      const gamepad_arrival_t &metadata
    ) {
      if (requested_profile != "auto"sv) {
        if (const auto profile = find_profile(requested_profile)) {
          return *profile;
        }
      }
      return automatic_profile(metadata);
    }

    float normalize_axis(std::int16_t value) {
      if (value < 0) {
        return std::max(-1.0F, static_cast<float>(value) / 32768.0F);
      }
      return std::min(1.0F, static_cast<float>(value) / 32767.0F);
    }

    float normalize_trigger(std::uint8_t value) {
      return static_cast<float>(value) / static_cast<float>(std::numeric_limits<std::uint8_t>::max());
    }

    lvh::ClientControllerType client_controller_type(std::uint8_t type) {
      using enum lvh::ClientControllerType;
      switch (type) {
        case LI_CTYPE_XBOX:
          return xbox;
        case LI_CTYPE_PS:
          return playstation;
        case LI_CTYPE_NINTENDO:
          return nintendo;
        case LI_CTYPE_UNKNOWN:
        default:
          return unknown;
      }
    }

    std::string random_private_mac() {
      std::random_device random;
      return std::format(
        "02:00:{:02x}:{:02x}:{:02x}:{:02x}",
        random() & 0xFF,
        random() & 0xFF,
        random() & 0xFF,
        random() & 0xFF
      );
    }

    std::string stable_id(
      const gamepad_id_t &id,
      const lvh::DeviceProfile &profile
    ) {
      if (profile.gamepad_kind != lvh::GamepadProfileKind::dualshock4 &&
          profile.gamepad_kind != lvh::GamepadProfileKind::dualsense) {
        return std::format("sunshine-gamepad-{}", id.globalIndex);
      }

      if (config::input.ds5_inputtino_randomize_mac || id.globalIndex < 0 || id.globalIndex > 255) {
        return random_private_mac();
      }
      return std::format("02:00:00:00:00:{:02x}", id.globalIndex);
    }

    lvh::GamepadMetadata gamepad_metadata(
      const gamepad_id_t &id,
      const gamepad_arrival_t &metadata,
      const lvh::DeviceProfile &profile
    ) {
      lvh::GamepadMetadata result;
      result.global_index = id.globalIndex;
      result.client_relative_index = id.clientRelativeIndex;
      result.client_type = client_controller_type(metadata.type);
      result.has_motion_sensors = metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO);
      result.has_touchpad = metadata.capabilities & LI_CCAP_TOUCHPAD;
      result.has_rgb_led = metadata.capabilities & LI_CCAP_RGB_LED;
      result.has_battery = metadata.capabilities & LI_CCAP_BATTERY_STATE;
      result.stable_id = stable_id(id, profile);
      return result;
    }

    void warn_feature_mismatch(
      int global_index,
      const gamepad_arrival_t &metadata,
      const lvh::GamepadProfileSupport &support
    ) {
      if ((metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO)) && !support.supports_motion) {
        BOOST_LOG(warning) << "Gamepad "sv << global_index << " has motion sensors, but the selected virtual profile cannot expose them"sv;
      }
      if ((metadata.capabilities & LI_CCAP_TOUCHPAD) && !support.supports_touchpad) {
        BOOST_LOG(warning) << "Gamepad "sv << global_index << " has a touchpad, but the selected virtual profile cannot expose it"sv;
      }
      if ((metadata.capabilities & LI_CCAP_RGB_LED) && !support.supports_rgb_led) {
        BOOST_LOG(warning) << "Gamepad "sv << global_index << " has an RGB LED, but the selected virtual profile cannot expose it"sv;
      }
      if (support.supports_motion && !(metadata.capabilities & (LI_CCAP_ACCEL | LI_CCAP_GYRO))) {
        BOOST_LOG(warning) << "Gamepad "sv << global_index << " is emulating a motion-capable controller, but the client did not enable motion sensors"sv;
      }
      if (support.supports_touchpad && !(metadata.capabilities & LI_CCAP_TOUCHPAD)) {
        BOOST_LOG(warning) << "Gamepad "sv << global_index << " is emulating a touchpad-capable controller, but the client has no touchpad"sv;
      }
    }

    lvh::GamepadState make_gamepad_state(
      const gamepad_state_t &state,
      const lvh::GamepadProfileSupport &support
    ) {
      using enum lvh::GamepadButton;

      lvh::GamepadState result;
      const auto flags = state.buttonFlags;
      result.buttons.set(dpad_up, flags & DPAD_UP);
      result.buttons.set(dpad_down, flags & DPAD_DOWN);
      result.buttons.set(dpad_left, flags & DPAD_LEFT);
      result.buttons.set(dpad_right, flags & DPAD_RIGHT);
      result.buttons.set(start, flags & START);
      result.buttons.set(back, flags & BACK);
      result.buttons.set(left_stick, flags & LEFT_STICK);
      result.buttons.set(right_stick, flags & RIGHT_STICK);
      result.buttons.set(left_shoulder, flags & LEFT_BUTTON);
      result.buttons.set(right_shoulder, flags & RIGHT_BUTTON);
      result.buttons.set(guide, flags & HOME);
      result.buttons.set(a, flags & A);
      result.buttons.set(b, flags & B);
      result.buttons.set(x, flags & X);
      result.buttons.set(y, flags & Y);
      result.buttons.set(misc1, support.supports_misc1_button && (flags & MISC_BUTTON));
      result.buttons.set(touchpad, support.supports_touchpad_button && (flags & TOUCHPAD_BUTTON));

      if (support.supports_touchpad_button && config::input.ds4_back_as_touchpad_click && (flags & BACK)) {
        result.buttons.set(touchpad);
      }

      result.left_stick = {.x = normalize_axis(state.lsX), .y = normalize_axis(state.lsY)};
      result.right_stick = {.x = normalize_axis(state.rsX), .y = normalize_axis(state.rsY)};
      result.left_trigger = normalize_trigger(state.lt);
      result.right_trigger = normalize_trigger(state.rt);
      return result;
    }

    lvh::GamepadBatteryState battery_state(std::uint8_t state) {
      using enum lvh::GamepadBatteryState;
      switch (state) {
        case LI_BATTERY_STATE_DISCHARGING:
          return discharging;
        case LI_BATTERY_STATE_CHARGING:
          return charging;
        case LI_BATTERY_STATE_FULL:
          return full;
        case LI_BATTERY_STATE_NOT_PRESENT:
        case LI_BATTERY_STATE_NOT_CHARGING:
          return charging_error;
        case LI_BATTERY_STATE_UNKNOWN:
        default:
          return unknown;
      }
    }

    void raise_feedback_unlocked(
      const std::shared_ptr<virtual_gamepad_backend_t::gamepad_context_t> &gamepad,
      const gamepad_feedback_msg_t &message
    ) {
      if (gamepad->feedback_queue) {
        gamepad->feedback_queue->raise(message);
      }
    }

    void raise_feedback(
      const std::shared_ptr<virtual_gamepad_backend_t::gamepad_context_t> &gamepad,
      const gamepad_feedback_msg_t &message
    ) {
      std::lock_guard lock {gamepad->feedback_mutex};
      raise_feedback_unlocked(gamepad, message);
    }

    void handle_output(
      const std::shared_ptr<virtual_gamepad_backend_t::gamepad_context_t> &gamepad,
      const lvh::GamepadOutput &output
    ) {
      std::lock_guard lock {gamepad->feedback_mutex};
      switch (output.kind) {
        case lvh::GamepadOutputKind::rumble:
          if (gamepad->has_last_rumble &&
              gamepad->last_low_frequency_rumble == output.low_frequency_rumble &&
              gamepad->last_high_frequency_rumble == output.high_frequency_rumble) {
            return;
          }
          gamepad->has_last_rumble = true;
          gamepad->last_low_frequency_rumble = output.low_frequency_rumble;
          gamepad->last_high_frequency_rumble = output.high_frequency_rumble;
          raise_feedback_unlocked(
            gamepad,
            gamepad_feedback_msg_t::make_rumble(
              gamepad->client_relative_index,
              output.low_frequency_rumble,
              output.high_frequency_rumble
            )
          );
          return;
        case lvh::GamepadOutputKind::trigger_rumble:
          if (gamepad->has_last_trigger_rumble &&
              gamepad->last_left_trigger_rumble == output.left_trigger_rumble &&
              gamepad->last_right_trigger_rumble == output.right_trigger_rumble) {
            return;
          }
          gamepad->has_last_trigger_rumble = true;
          gamepad->last_left_trigger_rumble = output.left_trigger_rumble;
          gamepad->last_right_trigger_rumble = output.right_trigger_rumble;
          raise_feedback_unlocked(
            gamepad,
            gamepad_feedback_msg_t::make_rumble_triggers(
              gamepad->client_relative_index,
              output.left_trigger_rumble,
              output.right_trigger_rumble
            )
          );
          return;
        case lvh::GamepadOutputKind::rgb_led:
          if (gamepad->has_last_rgb &&
              gamepad->last_red == output.red &&
              gamepad->last_green == output.green &&
              gamepad->last_blue == output.blue) {
            return;
          }
          gamepad->has_last_rgb = true;
          gamepad->last_red = output.red;
          gamepad->last_green = output.green;
          gamepad->last_blue = output.blue;
          raise_feedback_unlocked(
            gamepad,
            gamepad_feedback_msg_t::make_rgb_led(
              gamepad->client_relative_index,
              output.red,
              output.green,
              output.blue
            )
          );
          return;
        case lvh::GamepadOutputKind::adaptive_triggers:
          raise_feedback_unlocked(
            gamepad,
            gamepad_feedback_msg_t::make_adaptive_triggers(
              gamepad->client_relative_index,
              output.adaptive_trigger_flags,
              output.left_trigger_effect_type,
              output.right_trigger_effect_type,
              output.left_trigger_effect,
              output.right_trigger_effect
            )
          );
          return;
        case lvh::GamepadOutputKind::player_leds:
        case lvh::GamepadOutputKind::raw_report:
          return;
      }
    }

  }  // namespace

  virtual_gamepad_backend_t::virtual_gamepad_backend_t(lvh::BackendKind backend):
      gamepads_(static_cast<std::size_t>(MAX_GAMEPADS)) {
    lvh::RuntimeOptions options;
    options.backend = backend;
    runtime_ = lvh::Runtime::create(options);
    if (!runtime_) {
      BOOST_LOG(warning) << "Unable to create the virtual gamepad runtime"sv;
    }
  }

  virtual_gamepad_backend_t::~virtual_gamepad_backend_t() = default;

  int virtual_gamepad_backend_t::allocate(
    const gamepad_id_t &id,
    const gamepad_arrival_t &metadata,
    feedback_queue_t feedback_queue,
    std::string_view requested_profile
  ) {
    if (!runtime_ || !runtime_->capabilities().supports_gamepad) {
      return -1;
    }
    if (id.globalIndex < 0 || id.globalIndex >= static_cast<int>(gamepads_.size())) {
      BOOST_LOG(warning) << "Invalid virtual gamepad index: "sv << id.globalIndex;
      return -1;
    }

    const auto &selection = select_profile(requested_profile, metadata);
    auto profile = selection.profile();
    if (profile.gamepad_kind == lvh::GamepadProfileKind::switch_pro) {
      // Moonlight has no collision-free, negotiated player-indicator feedback
      // message. Do not advertise a feedback path Sunshine cannot forward.
      // Rumble, motion, battery, and the Switch home light remain supported.
      profile.capabilities.supports_player_leds = false;
    }
    profile.name = std::format("Sunshine {}", profile.name);
    if (requested_profile != "auto"sv) {
      BOOST_LOG(info) << "Gamepad "sv << id.globalIndex << " will be "sv << profile.name << " (manual selection)"sv;
    }

    lvh::CreateGamepadOptions options;
    options.profile = profile;
    options.metadata = gamepad_metadata(id, metadata, profile);
    auto created = lvh::GamepadStateAdapter::create(*runtime_, options);
    if (!created) {
      log_failure("create virtual gamepad"sv, created.status);
      return -1;
    }

    auto gamepad = std::make_shared<gamepad_context_t>();
    gamepad->adapter = std::move(created.adapter);
    gamepad->feedback_queue = std::move(feedback_queue);
    gamepad->client_relative_index = id.clientRelativeIndex;
    gamepad->profile_kind = selection.kind;
    gamepad->adapter->set_output_callback(
      [weak_gamepad = std::weak_ptr {gamepad}](const lvh::GamepadOutput &output) {
        if (const auto gamepad = weak_gamepad.lock()) {
          handle_output(gamepad, output);
        }
      }
    );

    const auto &support = gamepad->adapter->support();
    warn_feature_mismatch(id.globalIndex, metadata, support);
    if (support.supports_motion) {
      raise_feedback(
        gamepad,
        gamepad_feedback_msg_t::make_motion_event_state(
          id.clientRelativeIndex,
          LI_MOTION_TYPE_ACCEL,
          100
        )
      );
      raise_feedback(
        gamepad,
        gamepad_feedback_msg_t::make_motion_event_state(
          id.clientRelativeIndex,
          LI_MOTION_TYPE_GYRO,
          100
        )
      );
    }

    gamepads_[id.globalIndex] = std::move(gamepad);
    return 0;
  }

  void virtual_gamepad_backend_t::free(int nr) {
    if (has_gamepad(nr)) {
      gamepads_[nr].reset();
    }
  }

  bool virtual_gamepad_backend_t::has_gamepad(int nr) const {
    return nr >= 0 && nr < static_cast<int>(gamepads_.size()) &&
           gamepads_[nr] && gamepads_[nr]->adapter;
  }

  bool virtual_gamepad_backend_t::is_dualsense(int nr) const {
    return has_gamepad(nr) && gamepads_[nr]->profile_kind == lvh::GamepadProfileKind::dualsense;
  }

  lvh::Runtime *virtual_gamepad_backend_t::runtime() const {
    return runtime_.get();
  }

#ifdef SUNSHINE_TESTS
  lvh::GamepadStateAdapter *virtual_gamepad_backend_t::adapter_for_testing(int nr) const {
    return has_gamepad(nr) ? gamepads_[nr]->adapter.get() : nullptr;
  }
#endif

  void virtual_gamepad_backend_t::update(int nr, const gamepad_state_t &state) {
    if (!has_gamepad(nr)) {
      return;
    }

    auto &gamepad = gamepads_[nr];
    auto updated_state = make_gamepad_state(state, gamepad->adapter->support());
    const auto &cached_state = gamepad->adapter->state();
    updated_state.acceleration = cached_state.acceleration;
    updated_state.gyroscope = cached_state.gyroscope;
    updated_state.battery = cached_state.battery;
    updated_state.touchpad_contacts = cached_state.touchpad_contacts;
    log_failure("submit virtual gamepad state"sv, gamepad->adapter->set_state(updated_state));
  }

  void virtual_gamepad_backend_t::touch(const gamepad_touch_t &touch) {
    if (!has_gamepad(touch.id.globalIndex)) {
      return;
    }

    auto &gamepad = gamepads_[touch.id.globalIndex];
    if (!gamepad->adapter->support().supports_touchpad) {
      return;
    }

    if (touch.eventType == LI_TOUCH_EVENT_CANCEL_ALL) {
      for (std::size_t index = 0; index < gamepad->touch_ids.size(); ++index) {
        if (gamepad->touch_ids[index]) {
          log_failure(
            "release virtual gamepad touch"sv,
            gamepad->adapter->clear_touchpad_contact(index)
          );
          gamepad->touch_ids[index].reset();
        }
      }
      return;
    }

    auto slot = std::ranges::find(gamepad->touch_ids, touch.pointerId);
    if (touch.eventType == LI_TOUCH_EVENT_DOWN && slot == gamepad->touch_ids.end()) {
      slot = std::ranges::find_if(gamepad->touch_ids, [](const auto &id) {
        return !id.has_value();
      });
      if (slot == gamepad->touch_ids.end()) {
        BOOST_LOG(warning) << "No free virtual gamepad touch slots"sv;
        return;
      }
      *slot = touch.pointerId;
    }

    if (slot == gamepad->touch_ids.end()) {
      return;
    }

    const auto index = static_cast<std::size_t>(std::distance(gamepad->touch_ids.begin(), slot));
    if (touch.eventType == LI_TOUCH_EVENT_UP || touch.eventType == LI_TOUCH_EVENT_CANCEL) {
      log_failure(
        "release virtual gamepad touch"sv,
        gamepad->adapter->clear_touchpad_contact(index)
      );
      slot->reset();
      return;
    }
    if (touch.eventType != LI_TOUCH_EVENT_DOWN && touch.eventType != LI_TOUCH_EVENT_MOVE) {
      return;
    }

    lvh::GamepadTouchContact contact;
    contact.id = static_cast<std::uint8_t>(index);
    // Moonlight uses zero pressure when the client cannot measure it. The
    // transition type, not pressure, is authoritative for contact lifetime.
    contact.active = true;
    contact.x = std::clamp(touch.x, 0.0F, 1.0F);
    contact.y = std::clamp(touch.y, 0.0F, 1.0F);
    log_failure(
      "submit virtual gamepad touch"sv,
      gamepad->adapter->set_touchpad_contact(index, contact)
    );
  }

  void virtual_gamepad_backend_t::motion(const gamepad_motion_t &motion) {
    if (!has_gamepad(motion.id.globalIndex)) {
      return;
    }

    auto &adapter = gamepads_[motion.id.globalIndex]->adapter;
    switch (motion.motionType) {
      case LI_MOTION_TYPE_ACCEL:
        log_failure(
          "submit virtual gamepad acceleration"sv,
          adapter->set_acceleration(lvh::Vector3 {motion.x, motion.y, motion.z})
        );
        return;
      case LI_MOTION_TYPE_GYRO:
        log_failure(
          "submit virtual gamepad gyroscope"sv,
          adapter->set_gyroscope(lvh::Vector3 {motion.x, motion.y, motion.z})
        );
        return;
      default:
        return;
    }
  }

  void virtual_gamepad_backend_t::battery(const gamepad_battery_t &battery) {
    if (!has_gamepad(battery.id.globalIndex)) {
      return;
    }

    auto &adapter = gamepads_[battery.id.globalIndex]->adapter;
    if (battery.state == LI_BATTERY_STATE_UNKNOWN || battery.state == LI_BATTERY_STATE_NOT_PRESENT) {
      log_failure("clear virtual gamepad battery"sv, adapter->clear_battery());
      return;
    }

    lvh::GamepadBattery value;
    value.state = battery_state(battery.state);
    value.percentage = battery.percentage == LI_BATTERY_PERCENTAGE_UNKNOWN ?
                         100 :
                         std::min<std::uint8_t>(battery.percentage, 100);
    log_failure("submit virtual gamepad battery"sv, adapter->set_battery(value));
  }

  std::vector<supported_gamepad_t> static_supported_gamepads() {
    std::vector<supported_gamepad_t> gamepads;
    gamepads.reserve(gamepad_profiles.size() + 1);
    gamepads.emplace_back("auto", true, "");
    for (const auto &profile : gamepad_profiles) {
      gamepads.emplace_back(std::string {profile.name}, false, "");
    }
    return gamepads;
  }

  bool runtime_gamepads_available(lvh::Runtime *runtime) {
    if (!runtime || !runtime->capabilities().supports_gamepad) {
      return false;
    }
#ifdef _WIN32
    return !runtime->capabilities().requires_installed_driver ||
           lvh::get_license_status().license.licensed();
#else
    // The macOS backend always creates devices through its licensed broker,
    // even though the generic capability flag is currently false there.
    return lvh::get_license_status().license.licensed();
#endif
  }

  std::vector<supported_gamepad_t> supported_gamepads(lvh::Runtime *runtime) {
    const auto enabled = runtime_gamepads_available(runtime);
    const auto reason = enabled ? "" : "gamepads.virtualhid-broker-unavailable";

    std::vector<supported_gamepad_t> gamepads;
    gamepads.reserve(gamepad_profiles.size() + 1);
    gamepads.emplace_back("auto", enabled, reason);
    for (const auto &profile : gamepad_profiles) {
      gamepads.emplace_back(std::string {profile.name}, enabled, reason);
    }
    return gamepads;
  }

  bool profile_supports_controller_touch(std::string_view profile) {
    if (profile == "auto"sv) {
      return true;
    }
    const auto selected = find_profile(profile);
    if (!selected) {
      return false;
    }
    const auto support = lvh::gamepad_profile_support(selected->profile());
    return support.supports_touchpad;
  }

}  // namespace platf::virtualhid
