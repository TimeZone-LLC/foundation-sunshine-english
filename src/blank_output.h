/**
 * @file src/blank_output.h
 * @brief Declarations for the client-toggled blank output mode.
 * @details While blank output is enabled the capture threads stop pulling frames from the
 *          display and every streaming session sends a static black picture instead. The
 *          session, control channel, audio and input keep running, so the client can flip
 *          the mode back without renegotiating the stream. The state is process-wide and
 *          is never persisted; it resets when the last streaming session ends.
 */
#pragma once

#include <chrono>
#include <optional>
#include <string_view>

namespace blank_output {

  /**
   * @brief How often a capture thread re-checks the flag while it idles instead of capturing.
   */
  constexpr std::chrono::milliseconds capture_poll_interval { 100 };

  /**
   * @brief Cadence at which a blanked session re-sends its static black frame.
   * @details Slow enough to keep the encoder essentially idle, fast enough that the client
   *          keeps receiving video packets and never treats the stream as stalled.
   */
  constexpr std::chrono::milliseconds keepalive_frame_time { 500 };

  /**
   * @brief Whether blank output is currently enabled.
   */
  bool
  enabled();

  /**
   * @brief Enable or disable blank output.
   * @param value The new state.
   */
  void
  set(bool value);

  /**
   * @brief Flip the blank output state.
   * @return The state after the flip.
   */
  bool
  toggle();

  /**
   * @brief Disable blank output. Called when the last streaming session ends.
   */
  void
  reset();

  /**
   * @brief What changed between two consecutive observations of the flag.
   */
  enum class transition_e {
    none,  ///< The state is the same as last time.
    entered,  ///< Blank output was just switched on.
    exited  ///< Blank output was just switched off.
  };

  /**
   * @brief Detects transitions of the flag from the point of view of a single encode loop.
   * @details Starts from "not blanked", so a session that begins while the host is already
   *          blanked reports `entered` on its first update and converts the black frame immediately.
   */
  class edge_tracker_t {
  public:
    /**
     * @brief Record the current state and report what changed.
     * @param now The current value of blank_output::enabled().
     * @return The transition relative to the previous update.
     */
    transition_e
    update(bool now);

    /**
     * @brief The state recorded by the last update.
     */
    bool
    blanked() const;

  private:
    bool last_ = false;
  };

  /**
   * @brief Parse a boolean query parameter.
   * @param value The raw parameter text.
   * @return The parsed value, or no value when the text is not a recognised boolean.
   */
  std::optional<bool>
  parse_flag(std::string_view value);

}  // namespace blank_output
