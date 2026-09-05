/**
 * @file tests/unit/test_nvenc_rate_control.cpp
 * @brief Tests for the NVENC bitrate reconfiguration helper.
 */

#include <gtest/gtest.h>

#include <src/nvenc/common_impl/nvenc_rate_control.h>

namespace {

  NV_ENC_RC_PARAMS
  make_rc(NV_ENC_PARAMS_RC_MODE mode, std::uint32_t average, std::uint32_t max, std::uint32_t vbv) {
    NV_ENC_RC_PARAMS rc {};
    rc.rateControlMode = mode;
    rc.averageBitRate = average;
    rc.maxBitRate = max;
    rc.vbvBufferSize = vbv;
    return rc;
  }

}  // namespace

TEST(NvencRateControl, CbrKeepsMaxEqualToAverage) {
  auto rc = make_rc(NV_ENC_PARAMS_RC_CBR, 20'000'000, 20'000'000, 333'333);
  nvenc::rate_control::apply_bitrate(rc, 40'000'000);
  EXPECT_EQ(rc.averageBitRate, 40'000'000u);
  EXPECT_EQ(rc.maxBitRate, 40'000'000u);
}

TEST(NvencRateControl, VbrPreservesHeadroomRatio) {
  // Created with maxBitRate = 1.5x average; a bitrate change must not collapse it to CBR.
  auto rc = make_rc(NV_ENC_PARAMS_RC_VBR, 20'000'000, 30'000'000, 333'333);
  nvenc::rate_control::apply_bitrate(rc, 40'000'000);
  EXPECT_EQ(rc.averageBitRate, 40'000'000u);
  EXPECT_EQ(rc.maxBitRate, 60'000'000u);
}

TEST(NvencRateControl, VbvScalesWithBitrateForEveryCodec) {
  auto rc = make_rc(NV_ENC_PARAMS_RC_CBR, 20'000'000, 20'000'000, 333'333);
  nvenc::rate_control::apply_bitrate(rc, 10'000'000);
  EXPECT_EQ(rc.vbvBufferSize, 166'666u);
}

TEST(NvencRateControl, VbvNeverDropsBelowFloor) {
  auto rc = make_rc(NV_ENC_PARAMS_RC_CBR, 20'000'000, 20'000'000, 200'000);
  nvenc::rate_control::apply_bitrate(rc, 1'000'000);
  EXPECT_EQ(rc.vbvBufferSize, nvenc::rate_control::min_vbv_buffer_bits);
}

TEST(NvencRateControl, DriverDefaultVbvStaysDriverDefault) {
  auto rc = make_rc(NV_ENC_PARAMS_RC_CBR, 20'000'000, 20'000'000, 0);
  nvenc::rate_control::apply_bitrate(rc, 40'000'000);
  EXPECT_EQ(rc.vbvBufferSize, 0u);
}

TEST(NvencRateControl, ZeroPreviousAverageDoesNotDivide) {
  auto rc = make_rc(NV_ENC_PARAMS_RC_VBR, 0, 0, 333'333);
  nvenc::rate_control::apply_bitrate(rc, 40'000'000);
  EXPECT_EQ(rc.averageBitRate, 40'000'000u);
  EXPECT_EQ(rc.maxBitRate, 40'000'000u);
  EXPECT_EQ(rc.vbvBufferSize, 333'333u);
}
