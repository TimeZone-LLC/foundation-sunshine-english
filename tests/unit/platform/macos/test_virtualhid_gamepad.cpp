/**
 * @file tests/unit/platform/macos/test_virtualhid_gamepad.cpp
 * @brief Focused tests for the macOS libvirtualhid gamepad adapter.
 */

#if defined(__APPLE__) || defined(SUNSHINE_MACOS_VIRTUALHID_TEST_SYNTAX)

#include "../../../tests_common.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "src/config.h"
#include "src/globals.h"
#include "src/platform/macos/virtualhid_gamepad.h"

using namespace std::chrono_literals;
using namespace std::literals;

namespace {

  class MacosVirtualGamepadTest: public ::testing::Test {
  protected:
    void SetUp() override {
      original_input_ = config::input;
      config::input.ds4_back_as_touchpad_click = true;
      config::input.ds5_inputtino_randomize_mac = false;
      const auto *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
      feedback_ = mail::man->queue<platf::gamepad_feedback_msg_t>(
        std::string {"macos-virtual-gamepad-test-"} + test_info->name()
      );
    }

    void TearDown() override {
      feedback_.reset();
      config::input = std::move(original_input_);
    }

    config::input_t original_input_;
    platf::feedback_queue_t feedback_;
    platf::macos::virtual_gamepad_backend_t backend_ {lvh::BackendKind::fake};
  };

}  // namespace

TEST(MacosVirtualGamepadProfiles, ReportsAllProfilesAndExtensionSupport) {
  constexpr std::array expected_names {
    "auto"sv,
    "generic"sv,
    "x360"sv,
    "xone"sv,
    "xseries"sv,
    "ds4"sv,
    "ds5"sv,
    "switch"sv,
  };

  const auto profiles = platf::macos::static_supported_gamepads();
  ASSERT_EQ(profiles.size(), expected_names.size());
  for (std::size_t index = 0; index < expected_names.size(); ++index) {
    EXPECT_EQ(profiles[index].name, expected_names[index]);
  }

  EXPECT_TRUE(platf::macos::profile_supports_controller_extensions("auto"sv));
  EXPECT_TRUE(platf::macos::profile_supports_controller_extensions("ds4"sv));
  EXPECT_TRUE(platf::macos::profile_supports_controller_extensions("ds5"sv));
  EXPECT_TRUE(platf::macos::profile_supports_controller_extensions("switch"sv));
  EXPECT_FALSE(platf::macos::profile_supports_controller_extensions("x360"sv));
  EXPECT_FALSE(platf::macos::profile_supports_controller_extensions("unknown"sv));
}

TEST_F(MacosVirtualGamepadTest, AllocatesAndTranslatesFullControllerState) {
  const platf::gamepad_id_t id {1, 4};
  const auto capabilities = static_cast<std::uint16_t>(
    LI_CCAP_ACCEL | LI_CCAP_GYRO | LI_CCAP_TOUCHPAD | LI_CCAP_RGB_LED | LI_CCAP_BATTERY_STATE
  );
  const platf::gamepad_arrival_t metadata {LI_CTYPE_PS, capabilities, 0};

  ASSERT_EQ(backend_.allocate(id, metadata, feedback_, "ds5"sv), 0);
  EXPECT_TRUE(backend_.has_gamepad(id.globalIndex));
  EXPECT_TRUE(backend_.is_dualsense(id.globalIndex));

  auto *adapter = backend_.adapter_for_testing(id.globalIndex);
  ASSERT_NE(adapter, nullptr);
  ASSERT_NE(adapter->gamepad(), nullptr);
  EXPECT_EQ(adapter->gamepad()->profile().gamepad_kind, lvh::GamepadProfileKind::dualsense);
  EXPECT_EQ(adapter->gamepad()->metadata().stable_id, "02:00:00:00:00:01");

  const platf::gamepad_state_t input_state {
    platf::DPAD_UP | platf::START | platf::BACK | platf::A | platf::MISC_BUTTON,
    128,
    255,
    std::numeric_limits<std::int16_t>::min(),
    std::numeric_limits<std::int16_t>::max(),
    -16384,
    0,
  };
  backend_.update(id.globalIndex, input_state);

  const auto &state = adapter->state();
  EXPECT_TRUE(state.buttons.test(lvh::GamepadButton::dpad_up));
  EXPECT_TRUE(state.buttons.test(lvh::GamepadButton::start));
  EXPECT_TRUE(state.buttons.test(lvh::GamepadButton::back));
  EXPECT_TRUE(state.buttons.test(lvh::GamepadButton::a));
  EXPECT_TRUE(state.buttons.test(lvh::GamepadButton::misc1));
  EXPECT_TRUE(state.buttons.test(lvh::GamepadButton::touchpad));
  EXPECT_FLOAT_EQ(state.left_stick.x, -1.0F);
  EXPECT_FLOAT_EQ(state.left_stick.y, 1.0F);
  EXPECT_FLOAT_EQ(state.right_stick.x, -0.5F);
  EXPECT_FLOAT_EQ(state.left_trigger, 128.0F / 255.0F);
  EXPECT_FLOAT_EQ(state.right_trigger, 1.0F);

  backend_.free(id.globalIndex);
  EXPECT_FALSE(backend_.has_gamepad(id.globalIndex));
}

TEST_F(MacosVirtualGamepadTest, PreservesTouchMotionAndBatteryAcrossFullUpdates) {
  const platf::gamepad_id_t id {0, 2};
  const auto capabilities = static_cast<std::uint16_t>(
    LI_CCAP_ACCEL | LI_CCAP_GYRO | LI_CCAP_TOUCHPAD | LI_CCAP_BATTERY_STATE
  );
  ASSERT_EQ(
    backend_.allocate(id, {LI_CTYPE_PS, capabilities, 0}, feedback_, "ds5"sv),
    0
  );

  backend_.touch({id, LI_TOUCH_EVENT_DOWN, 42, 0.25F, 0.75F, 1.0F});
  backend_.motion({id, LI_MOTION_TYPE_ACCEL, 1.0F, 2.0F, 3.0F});
  backend_.motion({id, LI_MOTION_TYPE_GYRO, 4.0F, 5.0F, 6.0F});
  backend_.battery({id, LI_BATTERY_STATE_CHARGING, 73});
  backend_.update(id.globalIndex, {platf::A, 0, 0, 0, 0, 0, 0});

  const auto *adapter = backend_.adapter_for_testing(id.globalIndex);
  ASSERT_NE(adapter, nullptr);
  const auto &state = adapter->state();
  ASSERT_TRUE(state.acceleration);
  EXPECT_FLOAT_EQ(state.acceleration->x, 1.0F);
  ASSERT_TRUE(state.gyroscope);
  EXPECT_FLOAT_EQ(state.gyroscope->z, 6.0F);
  ASSERT_TRUE(state.battery);
  EXPECT_EQ(state.battery->state, lvh::GamepadBatteryState::charging);
  EXPECT_EQ(state.battery->percentage, 73);
  EXPECT_TRUE(state.touchpad_contacts[0].active);
  EXPECT_FLOAT_EQ(state.touchpad_contacts[0].x, 0.25F);
  EXPECT_TRUE(state.buttons.test(lvh::GamepadButton::a));
}

TEST_F(MacosVirtualGamepadTest, RoutesAndDeduplicatesRumbleFeedback) {
  const platf::gamepad_id_t id {0, 3};
  ASSERT_EQ(backend_.allocate(id, {LI_CTYPE_XBOX, 0, 0}, feedback_, "xseries"sv), 0);

  lvh::GamepadOutput output;
  output.kind = lvh::GamepadOutputKind::rumble;
  output.low_frequency_rumble = 100;
  output.high_frequency_rumble = 200;

  auto *adapter = backend_.adapter_for_testing(id.globalIndex);
  ASSERT_NE(adapter, nullptr);
  ASSERT_TRUE(adapter->dispatch_output(output).ok());
  auto feedback = feedback_->pop(10ms);
  ASSERT_TRUE(feedback);
  EXPECT_EQ(feedback->type, platf::gamepad_feedback_e::rumble);
  EXPECT_EQ(feedback->id, id.clientRelativeIndex);
  EXPECT_EQ(feedback->data.rumble.lowfreq, 100);
  EXPECT_EQ(feedback->data.rumble.highfreq, 200);

  ASSERT_TRUE(adapter->dispatch_output(output).ok());
  EXPECT_FALSE(feedback_->pop(0ms));
}

#endif  // defined(__APPLE__) || defined(SUNSHINE_MACOS_VIRTUALHID_TEST_SYNTAX)
