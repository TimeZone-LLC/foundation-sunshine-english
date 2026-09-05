/**
 * @file tests/unit/test_blank_output.cpp
 * @brief Tests for the client-toggled blank output mode.
 */

#include <gtest/gtest.h>

#include <src/blank_output.h>

namespace {

  class BlankOutputTest: public ::testing::Test {
  protected:
    void
    SetUp() override {
      blank_output::reset();
    }

    void
    TearDown() override {
      blank_output::reset();
    }
  };

}  // namespace

TEST_F(BlankOutputTest, DisabledByDefault) {
  EXPECT_FALSE(blank_output::enabled());
}

TEST_F(BlankOutputTest, SetEnablesAndResetDisables) {
  blank_output::set(true);
  EXPECT_TRUE(blank_output::enabled());

  blank_output::reset();
  EXPECT_FALSE(blank_output::enabled());
}

TEST_F(BlankOutputTest, ToggleFlipsAndReturnsNewState) {
  EXPECT_TRUE(blank_output::toggle());
  EXPECT_TRUE(blank_output::enabled());

  EXPECT_FALSE(blank_output::toggle());
  EXPECT_FALSE(blank_output::enabled());
}

TEST(BlankOutputEdgeTracker, ReportsNoTransitionWhileUnchanged) {
  blank_output::edge_tracker_t tracker;
  EXPECT_EQ(tracker.update(false), blank_output::transition_e::none);
  EXPECT_EQ(tracker.update(false), blank_output::transition_e::none);
  EXPECT_FALSE(tracker.blanked());
}

TEST(BlankOutputEdgeTracker, ReportsEnteredThenExited) {
  blank_output::edge_tracker_t tracker;
  EXPECT_EQ(tracker.update(true), blank_output::transition_e::entered);
  EXPECT_TRUE(tracker.blanked());
  EXPECT_EQ(tracker.update(true), blank_output::transition_e::none);
  EXPECT_EQ(tracker.update(false), blank_output::transition_e::exited);
  EXPECT_FALSE(tracker.blanked());
}

TEST(BlankOutputEdgeTracker, SessionStartingWhileBlankedEntersOnFirstUpdate) {
  // A session that starts while the host is already blanked must convert the
  // black frame right away instead of waiting for a later toggle.
  blank_output::edge_tracker_t tracker;
  EXPECT_EQ(tracker.update(true), blank_output::transition_e::entered);
}

TEST(BlankOutputParseFlag, AcceptsNumericAndWordForms) {
  EXPECT_EQ(blank_output::parse_flag("1"), std::optional<bool> { true });
  EXPECT_EQ(blank_output::parse_flag("0"), std::optional<bool> { false });
  EXPECT_EQ(blank_output::parse_flag("true"), std::optional<bool> { true });
  EXPECT_EQ(blank_output::parse_flag("FALSE"), std::optional<bool> { false });
  EXPECT_EQ(blank_output::parse_flag("on"), std::optional<bool> { true });
  EXPECT_EQ(blank_output::parse_flag("Off"), std::optional<bool> { false });
  EXPECT_EQ(blank_output::parse_flag("yes"), std::optional<bool> { true });
  EXPECT_EQ(blank_output::parse_flag("no"), std::optional<bool> { false });
}

TEST(BlankOutputParseFlag, RejectsUnknownValues) {
  EXPECT_EQ(blank_output::parse_flag(""), std::nullopt);
  EXPECT_EQ(blank_output::parse_flag("maybe"), std::nullopt);
  EXPECT_EQ(blank_output::parse_flag("2"), std::nullopt);
}
