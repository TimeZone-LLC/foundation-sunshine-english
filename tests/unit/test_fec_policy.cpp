/**
 * @file tests/unit/test_fec_policy.cpp
 * @brief Tests for the per-frame forward error correction policy.
 */

#include <gtest/gtest.h>

#include <src/fec_policy.h>

TEST(FecPolicy, OffStaysOff) {
  EXPECT_EQ(stream::fec_policy::percentage_for_frame(0, 100, 0), 0);
}

TEST(FecPolicy, LargeFrameKeepsConfiguredPercentage) {
  EXPECT_EQ(stream::fec_policy::percentage_for_frame(5, 100, 0), 5);
  EXPECT_EQ(stream::fec_policy::percentage_for_frame(20, stream::fec_policy::min_data_packets_for_parity, 0), 20);
}

TEST(FecPolicy, TinyFrameSkipsParity) {
  // A one or two packet frame would otherwise carry 50 to 100 percent overhead.
  EXPECT_EQ(stream::fec_policy::percentage_for_frame(5, 1, 0), 0);
  EXPECT_EQ(stream::fec_policy::percentage_for_frame(20, stream::fec_policy::min_data_packets_for_parity - 1, 0), 0);
}

TEST(FecPolicy, ClientDemandedParityOverridesTinyFrameSkip) {
  // Moonlight can require a minimum number of parity packets; honour it.
  EXPECT_EQ(stream::fec_policy::percentage_for_frame(5, 1, 2), 5);
}
