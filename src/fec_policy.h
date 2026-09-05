/**
 * @file src/fec_policy.h
 * @brief Per-frame forward error correction policy for the video sender.
 */
#pragma once

#include <cstddef>

namespace stream::fec_policy {

  /**
   * @brief Frames with fewer data packets than this carry no parity packets.
   * @details A one or two packet frame would otherwise pay 50 to 100 percent overhead for a
   *          single parity packet, and the client recovers such a loss faster by reference
   *          frame invalidation than by waiting for parity anyway.
   */
  constexpr std::size_t min_data_packets_for_parity = 4;

  /**
   * @brief Decide the FEC percentage for one encoded frame.
   * @param base_percentage The configured percentage for this peer (LAN or WAN value).
   * @param data_packets The number of data packets the frame splits into.
   * @param client_min_parity_packets The parity minimum the client asked for, 0 when it did not.
   * @return The percentage to use; 0 disables parity for this frame.
   */
  int
  percentage_for_frame(int base_percentage, std::size_t data_packets, int client_min_parity_packets);

}  // namespace stream::fec_policy
