/**
 * @file tests/unit/test_sunshinesvc_state.cpp
 * @brief Tests for the GUI agent and Sunshine.exe lifecycle policies used by sunshinesvc.
 */
#include <tools/sunshinesvc_state.h>

#include <array>
#include <gtest/gtest.h>

TEST(GuiRestartBackoff, BacksOffRepeatedFailures) {
  sunshinesvc::GuiRestartBackoff backoff;
  constexpr std::array expected { 1000U, 2000U, 4000U, 8000U, 16000U, 30000U, 30000U };

  for (std::size_t index = 0; index < expected.size(); ++index) {
    SCOPED_TRACE(index);
    EXPECT_EQ(backoff.next_delay(), expected[index]);
  }
}

TEST(GuiRestartBackoff, ResetsOnlyAfterStableRuntime) {
  sunshinesvc::GuiRestartBackoff backoff;

  EXPECT_EQ(backoff.next_delay(), 1000U);
  EXPECT_EQ(backoff.next_delay(), 2000U);
  EXPECT_EQ(backoff.next_delay(sunshinesvc::GUI_RESTART_STABLE_RUNTIME_MS - 1), 4000U);
  EXPECT_EQ(backoff.next_delay(sunshinesvc::GUI_RESTART_STABLE_RUNTIME_MS), 1000U);
  EXPECT_EQ(backoff.next_delay(), 2000U);
}

TEST(GuiRestartBackoff, ExplicitResetStartsAtInitialDelay) {
  sunshinesvc::GuiRestartBackoff backoff;

  EXPECT_EQ(backoff.next_delay(), 1000U);
  EXPECT_EQ(backoff.next_delay(), 2000U);
  backoff.reset();
  EXPECT_EQ(backoff.next_delay(), 1000U);
}

TEST(CoreRestartBackoff, BacksOffRepeatedFailures) {
  sunshinesvc::CoreRestartBackoff backoff;
  constexpr std::array expected { 3000U, 6000U, 12000U, 24000U, 48000U, 60000U, 60000U };

  for (std::size_t index = 0; index < expected.size(); ++index) {
    SCOPED_TRACE(index);
    EXPECT_EQ(backoff.next_delay(), expected[index]);
  }
}

TEST(CoreRestartBackoff, StartsAtTheInitialDelay) {
  sunshinesvc::CoreRestartBackoff backoff;

  EXPECT_EQ(backoff.next_delay(), sunshinesvc::CORE_RESTART_INITIAL_DELAY_MS);
}

TEST(CoreRestartBackoff, SaturatesAtTheCeiling) {
  sunshinesvc::CoreRestartBackoff backoff;

  // A long-running failure streak must keep hitting the ceiling instead of
  // wrapping back around to a tiny delay.
  for (std::size_t index = 0; index < 64; ++index) {
    SCOPED_TRACE(index);
    EXPECT_LE(backoff.next_delay(), sunshinesvc::CORE_RESTART_MAX_DELAY_MS);
  }
  EXPECT_EQ(backoff.next_delay(), sunshinesvc::CORE_RESTART_MAX_DELAY_MS);
}

TEST(CoreRestartBackoff, ResetsOnlyAfterStableRuntime) {
  sunshinesvc::CoreRestartBackoff backoff;

  EXPECT_EQ(backoff.next_delay(), 3000U);
  EXPECT_EQ(backoff.next_delay(), 6000U);
  EXPECT_EQ(backoff.next_delay(sunshinesvc::CORE_RESTART_STABLE_RUNTIME_MS - 1), 12000U);
  EXPECT_EQ(backoff.next_delay(sunshinesvc::CORE_RESTART_STABLE_RUNTIME_MS), 3000U);
  EXPECT_EQ(backoff.next_delay(), 6000U);
}

TEST(CoreRestartBackoff, ExplicitResetStartsAtInitialDelay) {
  sunshinesvc::CoreRestartBackoff backoff;

  EXPECT_EQ(backoff.next_delay(), 3000U);
  EXPECT_EQ(backoff.next_delay(), 6000U);
  backoff.reset();
  EXPECT_EQ(backoff.next_delay(), 3000U);
}

TEST(ClassifyCoreExit, TreatsOnlyZeroAsACleanStop) {
  EXPECT_EQ(sunshinesvc::classify_core_exit(0), sunshinesvc::CoreExitKind::clean);
  EXPECT_EQ(sunshinesvc::classify_core_exit(1), sunshinesvc::CoreExitKind::failed);
  EXPECT_EQ(sunshinesvc::classify_core_exit(0xFFFFFFFFU), sunshinesvc::CoreExitKind::failed);

  static_assert(sunshinesvc::classify_core_exit(sunshinesvc::CORE_CLEAN_EXIT_CODE) ==
                sunshinesvc::CoreExitKind::clean);
}

TEST(GuiAgentRestartPolicy, CleanExitSuppressesLaunchUntilSupervisionResumes) {
  sunshinesvc::GuiAgentRestartPolicy policy;

  EXPECT_TRUE(policy.launch_allowed());
  policy.suppress_launch();
  EXPECT_FALSE(policy.launch_allowed());
  policy.resume_supervision();
  EXPECT_TRUE(policy.launch_allowed());
}

TEST(GuiAgentCrashLogLimiter, LogsStateChangesAndPeriodicReminders) {
  sunshinesvc::GuiAgentCrashLogLimiter limiter;

  EXPECT_TRUE(limiter.should_log_acquisition(false));
  EXPECT_FALSE(limiter.should_log_acquisition(false));

  EXPECT_TRUE(limiter.should_log_exit(1, 1000, 0));
  EXPECT_TRUE(limiter.should_log_acquisition(false));
  EXPECT_FALSE(limiter.should_log_exit(1, 1000, sunshinesvc::GUI_CRASH_LOG_REMINDER_MS - 1));
  EXPECT_FALSE(limiter.should_log_acquisition(false));
  EXPECT_TRUE(limiter.should_log_exit(1, 1000, sunshinesvc::GUI_CRASH_LOG_REMINDER_MS));
  EXPECT_TRUE(limiter.should_log_acquisition(false));

  EXPECT_TRUE(limiter.should_log_exit(1, 2000, sunshinesvc::GUI_CRASH_LOG_REMINDER_MS + 1));
  EXPECT_TRUE(limiter.should_log_exit(2, 2000, sunshinesvc::GUI_CRASH_LOG_REMINDER_MS + 2));
  EXPECT_TRUE(limiter.should_log_acquisition(false));
  EXPECT_TRUE(limiter.should_log_acquisition(true));

  limiter.reset();
  EXPECT_TRUE(limiter.should_log_acquisition(false));
}
