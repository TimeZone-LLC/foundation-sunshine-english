/**
 * @file tests/unit/test_permissions.cpp
 * @brief Tests for shared permission policy.
 */

#include "../tests_common.h"

#include "src/platform/permissions.h"

TEST(PlatformPermissionsTest, RequiredPermissionsIgnoreOptionalAndOnUseStatuses) {
  using platf::permission_status_t;
  EXPECT_TRUE(platf::required_permissions_granted({
    permission_status_t { "screen_recording", "granted", true, true },
    permission_status_t { "local_network", "on_use", true, false },
    permission_status_t { "microphone", "denied", false, true },
  }));
  EXPECT_FALSE(platf::required_permissions_granted({
    permission_status_t { "screen_recording", "denied", true, true },
  }));
  EXPECT_TRUE(platf::required_permissions_granted({}));
}
