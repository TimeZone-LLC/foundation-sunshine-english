/**
 * @file src/blank_output.cpp
 * @brief Definitions for the client-toggled blank output mode.
 */
#include "blank_output.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <string>

namespace blank_output {

  namespace {
    std::atomic<bool> state { false };
  }  // namespace

  bool
  enabled() {
    return state.load(std::memory_order_acquire);
  }

  void
  set(bool value) {
    state.store(value, std::memory_order_release);
  }

  bool
  toggle() {
    bool expected = state.load(std::memory_order_acquire);
    while (!state.compare_exchange_weak(expected, !expected, std::memory_order_acq_rel, std::memory_order_acquire)) {
    }
    return !expected;
  }

  void
  reset() {
    set(false);
  }

  transition_e
  edge_tracker_t::update(bool now) {
    const bool was = last_;
    last_ = now;
    if (now == was) {
      return transition_e::none;
    }
    return now ? transition_e::entered : transition_e::exited;
  }

  bool
  edge_tracker_t::blanked() const {
    return last_;
  }

  std::optional<bool>
  parse_flag(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });

    if (lowered == "1" || lowered == "true" || lowered == "on" || lowered == "yes") {
      return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "off" || lowered == "no") {
      return false;
    }
    return std::nullopt;
  }

}  // namespace blank_output
