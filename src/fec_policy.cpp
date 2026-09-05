/**
 * @file src/fec_policy.cpp
 * @brief Definitions for the per-frame forward error correction policy.
 */
#include "fec_policy.h"

namespace stream::fec_policy {

  int
  percentage_for_frame(int base_percentage, std::size_t data_packets, int client_min_parity_packets) {
    if (base_percentage <= 0) {
      return 0;
    }
    if (client_min_parity_packets > 0) {
      return base_percentage;
    }
    if (data_packets < min_data_packets_for_parity) {
      return 0;
    }
    return base_percentage;
  }

}  // namespace stream::fec_policy
