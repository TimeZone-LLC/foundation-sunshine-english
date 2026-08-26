/**
 * @file src/rtsp.h
 * @brief Declarations for RTSP streaming.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <boost/function.hpp>
#include <boost/process/v1.hpp>

#include "crypto.h"
#include "hdr/client_display_capabilities.h"
#include "launch_session_manager.h"

namespace stream::session {
  enum class stop_reason_e : int;
}

namespace rtsp_stream {
  constexpr auto RTSP_SETUP_PORT = 21;

  struct launch_session_t {
    uint32_t id;

    crypto::aes_t gcm_key;
    crypto::aes_t iv;

    std::string av_ping_payload;
    uint32_t control_connect_data;
    std::string client_cert_uuid;
    std::string rtsp_peer_address;
    bool highly_suspected_unknown_client {false};

    boost::process::v1::environment env;

    bool host_audio;
    std::string unique_id;
    std::string client_name;
    int width;
    int height;
    int fps;
    int gcmap;
    int appid;
    int surround_info;
    std::string surround_params;
    bool continuous_audio;
    bool enable_hdr;
    bool enable_sops;
    bool enable_mic { false };
    bool use_vdd;
    int custom_screen_mode;
    hdr::client_display_capabilities_t reported_hdr_capabilities;
    hdr::client_display_capabilities_t hdr_capabilities;
    hdr::target_source_e hdr_target_source { hdr::target_source_e::safe_defaults };

    void
    set_hdr_target(const hdr::client_display_capabilities_t &capabilities, hdr::target_source_e source);

    void
    sync_hdr_environment();

    std::optional<crypto::cipher::gcm_t> rtsp_cipher;
    std::string rtsp_url_scheme;
    uint32_t rtsp_iv_counter;

    // 跟踪已设置的流类型
    bool setup_video { false };
    bool setup_audio { false };
    bool setup_control { false };
    bool setup_mic { false };
    bool control_only { false };
    // Captured when /launch or /resume is accepted so a live UI toggle cannot
    // change the policy halfway through this RTSP handshake.
    bool input_only_mode { false };

    // GameStream 的 RTSP 请求可能分别使用独立 TCP 连接，
    // 但一个启动票据最多只能创建一个串流会话。
    bool stream_session_started { false };
    std::string stream_announce_payload;
  };

  /**
   * @brief Return true when the client requested only the control stream.
   */
  inline bool
  is_control_only_handshake(const launch_session_t &session) {
    return session.setup_control && !session.setup_video && !session.setup_audio && !session.setup_mic;
  }

  /**
   * @brief Return true when input-only mode should provide a compatibility video stream.
   */
  inline bool
  uses_input_only_keepalive_video(const launch_session_t &session) {
    return session.input_only_mode && session.setup_control && session.setup_video;
  }

  launch_ticket_register_e
  launch_session_raise(std::shared_ptr<launch_session_t> launch_session);

  /**
   * @brief Clear state for the specified launch session.
   * @param launch_session_id The ID of the session to clear.
   */
  void
  launch_session_clear(uint32_t launch_session_id);

  /**
   * @brief Get the number of active sessions.
   * @return Count of active sessions.
   */
  int
  session_count();

  /**
   * @brief Get the number of bounded launch tickets awaiting RTSP activation.
   */
  int
  pending_session_count();

  /**
   * @brief Terminates all streaming sessions on the RTSP execution context.
   * @param reason Reason recorded for sessions that are still running.
   * @param completion Called after the termination attempt finishes.
   */
  void
  terminate_sessions_async(stream::session::stop_reason_e reason, boost::function<void()> completion);

  /**
   * @brief Runs the RTSP server loop.
   */
  void start();
}  // namespace rtsp_stream
