/**
 * @file tests/unit/platform/macos/test_microphone_permission.mm
 * @brief Tests for the macOS microphone permission resolver.
 */

#include "../../../tests_common.h"

#import <AVFoundation/AVFoundation.h>

#include "src/platform/macos/av_audio.h"

TEST(MacosMicrophonePermissionTest, AuthorizedAccessDoesNotPromptAgain) {
  bool request_called = false;

  EXPECT_TRUE(platf::request_microphone_permission(
    AVAuthorizationStatusAuthorized,
    [&](platf::microphone_permission_callback_t callback) {
      request_called = true;
      callback(false);
    }
  ));
  EXPECT_FALSE(request_called);
}

TEST(MacosMicrophonePermissionTest, UnavailableAccessDoesNotPromptAgain) {
  bool request_called = false;
  const auto request_access = [&](platf::microphone_permission_callback_t callback) {
    request_called = true;
    callback(true);
  };

  EXPECT_FALSE(platf::request_microphone_permission(AVAuthorizationStatusDenied, request_access));
  EXPECT_FALSE(platf::request_microphone_permission(AVAuthorizationStatusRestricted, request_access));
  EXPECT_FALSE(platf::request_microphone_permission(static_cast<AVAuthorizationStatus>(-1), request_access));
  EXPECT_FALSE(request_called);
}

TEST(MacosMicrophonePermissionTest, UndeterminedAccessReturnsTheUsersDecision) {
  bool request_called = false;

  EXPECT_TRUE(platf::request_microphone_permission(
    AVAuthorizationStatusNotDetermined,
    [&](platf::microphone_permission_callback_t callback) {
      request_called = true;
      callback(true);
    }
  ));
  EXPECT_TRUE(request_called);

  request_called = false;
  EXPECT_FALSE(platf::request_microphone_permission(
    AVAuthorizationStatusNotDetermined,
    [&](platf::microphone_permission_callback_t callback) {
      request_called = true;
      callback(false);
    }
  ));
  EXPECT_TRUE(request_called);
}
