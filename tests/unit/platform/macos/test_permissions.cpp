/**
 * @file tests/unit/platform/macos/test_permissions.cpp
 * @brief Tests for macOS startup permission policy.
 */

#ifdef __APPLE__
  #include "../../../tests_common.h"

  #include "src/platform/macos/misc.h"

TEST(MacosPermissionsTest, StartupRequestsOnlyMissingPermissionsInUse) {
  EXPECT_TRUE(platf::should_request_startup_permission({ "screen_recording", "denied", true, true }));
  EXPECT_TRUE(platf::should_request_startup_permission({ "input", "not_determined", true, true }));
  EXPECT_TRUE(platf::should_request_startup_permission({ "microphone", "not_determined", true, true }));
  EXPECT_FALSE(platf::should_request_startup_permission({ "screen_recording", "granted", true, true }));
  EXPECT_FALSE(platf::should_request_startup_permission({ "input", "denied", false, true }));
  EXPECT_FALSE(platf::should_request_startup_permission({ "microphone", "denied", false, true }));
  EXPECT_FALSE(platf::should_request_startup_permission({ "local_network", "on_use", true, false }));
}

TEST(MacosPermissionsTest, CoreGraphicsPromptOnlyOnFirstStartupRequest) {
  using platf::permission_request_action_t;
  EXPECT_EQ(platf::permission_request_action(true, 0, true, false), permission_request_action_t::none);
  EXPECT_EQ(platf::permission_request_action(true, 1, false, true), permission_request_action_t::none);
  EXPECT_EQ(platf::permission_request_action(false, 0, true, false), permission_request_action_t::prompt);
  EXPECT_EQ(platf::permission_request_action(false, 1, true, true), permission_request_action_t::none);
  EXPECT_EQ(platf::permission_request_action(false, 1, false, false), permission_request_action_t::settings);
  EXPECT_EQ(platf::permission_request_action(false, 1, false, true), permission_request_action_t::prompt);
  EXPECT_EQ(platf::permission_request_action(false, 2, true, false), permission_request_action_t::settings);
}

TEST(MacosPermissionsTest, LocalNetworkPrivacyStartsWithMacOS15) {
  EXPECT_FALSE(platf::supports_local_network_privacy(14));
  EXPECT_TRUE(platf::supports_local_network_privacy(15));
  EXPECT_TRUE(platf::supports_local_network_privacy(27));
}
#endif
