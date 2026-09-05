/**
 * @file src/nvenc/common_impl/nvenc_rate_control.h
 * @brief Rate control arithmetic shared by NVENC encoder creation and reconfiguration.
 * @details Header-only so it compiles against whichever NVENC SDK line the including
 *          translation unit was built for.
 */
#pragma once

#include <algorithm>
#include <cstdint>

#include <ffnvcodec/nvEncodeAPI.h>

namespace nvenc::rate_control {

  /**
   * @brief Smallest VBV buffer a reconfiguration may leave behind, in bits.
   */
  constexpr std::uint32_t min_vbv_buffer_bits = 100 * 1000;

  /**
   * @brief Re-target the rate control parameters at a new average bitrate.
   * @details Keeps the mode's headroom: CBR stays at maxBitRate == averageBitRate, VBR keeps
   *          the ratio it was created with instead of collapsing into CBR. The VBV buffer
   *          scales with the bitrate for every codec so the configured buffering in frames is
   *          unchanged; a driver-default (zero) buffer stays driver-default.
   * @param rc The parameters currently applied to the encoder, updated in place.
   * @param new_average_bps The new average bitrate in bits per second.
   */
  inline void
  apply_bitrate(NV_ENC_RC_PARAMS &rc, std::uint32_t new_average_bps) {
    const std::uint64_t old_average = rc.averageBitRate;
    const std::uint64_t old_max = rc.maxBitRate;
    const std::uint64_t old_vbv = rc.vbvBufferSize;

    rc.averageBitRate = new_average_bps;

    const bool keeps_headroom = rc.rateControlMode == NV_ENC_PARAMS_RC_VBR && old_average > 0 && old_max > old_average;
    rc.maxBitRate = keeps_headroom ? static_cast<std::uint32_t>(new_average_bps * old_max / old_average) : new_average_bps;

    if (old_vbv > 0 && old_average > 0) {
      const auto scaled = static_cast<std::uint32_t>(new_average_bps * old_vbv / old_average);
      rc.vbvBufferSize = std::max(scaled, min_vbv_buffer_bits);
    }
  }

}  // namespace nvenc::rate_control
