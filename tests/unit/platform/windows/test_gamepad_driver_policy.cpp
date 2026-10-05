/**
 * @file tests/unit/platform/windows/test_gamepad_driver_policy.cpp
 * @brief Focused tests for Windows gamepad backend selection.
 */

#include <gtest/gtest.h>

#include "src/platform/windows/gamepad_driver_policy.h"

using namespace std::literals;

TEST(GamepadDriverPolicy, DefaultsUnknownValuesToExistingBackend) {
  using namespace platf::windows::gamepad_driver;

  EXPECT_EQ(parse("vigembus"sv), policy_e::vigembus);
  EXPECT_EQ(parse("virtualhid"sv), policy_e::virtualhid);
  EXPECT_EQ(parse("all"sv), policy_e::all);
  EXPECT_EQ(parse(""sv), policy_e::vigembus);
  EXPECT_EQ(parse("unknown"sv), policy_e::vigembus);
  EXPECT_FALSE(prefers_virtualhid(policy_e::vigembus));
  EXPECT_TRUE(prefers_virtualhid(policy_e::virtualhid));
  EXPECT_TRUE(prefers_virtualhid(policy_e::all));

  EXPECT_TRUE(eagerly_initializes_existing_backend(policy_e::vigembus, false));
  EXPECT_TRUE(eagerly_initializes_existing_backend(policy_e::vigembus, true));
  EXPECT_TRUE(eagerly_initializes_existing_backend(policy_e::all, false));
  EXPECT_TRUE(eagerly_initializes_existing_backend(policy_e::all, true));
  EXPECT_TRUE(eagerly_initializes_existing_backend(policy_e::virtualhid, false));
  EXPECT_FALSE(eagerly_initializes_existing_backend(policy_e::virtualhid, true));
}

TEST(GamepadDriverPolicy, KeepsFallbackAvailableForEveryPolicy) {
  using namespace platf::windows::gamepad_driver;

  EXPECT_TRUE(allows_existing_fallback(policy_e::vigembus));
  EXPECT_TRUE(allows_existing_fallback(policy_e::virtualhid));
  EXPECT_TRUE(allows_existing_fallback(policy_e::all));
}

TEST(GamepadDriverPolicy, ReportsProfileCoverage) {
  using namespace platf::windows::gamepad_driver;

  for (const auto profile : {"auto"sv, "generic"sv, "x360"sv, "xone"sv,
                             "xseries"sv, "ds4"sv, "ds5"sv, "switch"sv}) {
    EXPECT_TRUE(virtualhid_supports_profile(profile)) << profile;
  }
  EXPECT_FALSE(virtualhid_supports_profile("unknown"sv));

  EXPECT_TRUE(vigembus_supports_profile("auto"sv));
  EXPECT_TRUE(vigembus_supports_profile("x360"sv));
  EXPECT_TRUE(vigembus_supports_profile("ds4"sv));
  EXPECT_FALSE(vigembus_supports_profile("xone"sv));
  EXPECT_FALSE(vigembus_supports_profile("switch"sv));

  EXPECT_TRUE(existing_backend_supports_controller_touch("auto"sv));
  EXPECT_TRUE(existing_backend_supports_controller_touch("ds4"sv));
  EXPECT_TRUE(existing_backend_supports_controller_touch("ds5"sv));
  EXPECT_FALSE(existing_backend_supports_controller_touch("x360"sv));
  EXPECT_FALSE(existing_backend_supports_controller_touch("xseries"sv));
  EXPECT_FALSE(existing_backend_supports_controller_touch("switch"sv));
}
