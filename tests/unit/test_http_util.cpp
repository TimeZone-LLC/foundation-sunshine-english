/**
 * @file tests/unit/test_http_util.cpp
 * @brief Tests for shared HTTP request validation helpers.
 */
#include <gtest/gtest.h>

#include <array>
#include <map>
#include <sstream>

#include <src/http_util.h>

using namespace std::literals;

TEST(HttpUtilTest, MatchesNormalizedMediaType) {
  EXPECT_TRUE(http_util::content_type_matches(" Application/JSON; charset=utf-8", "application/json"));
  EXPECT_TRUE(http_util::content_type_matches("text/plain", "TEXT/PLAIN"));
  EXPECT_FALSE(http_util::content_type_matches("text/plain", "application/json"));
}

TEST(HttpUtilTest, LogsOnlyAllowedRequestFields) {
  const std::multimap<std::string, std::string> fields {
    { "appid", "123" },
    { "clientname", "Living room" },
    { "mode", "1920x1080x60\r\nforged" },
    { "rikey", "secret" },
    { "uniqueid", "client-01" },
  };
  static constexpr std::array allowed_names {
    "appid"sv,
    "clientname"sv,
    "mode"sv,
    "uniqueid"sv,
  };
  std::ostringstream output;

  http_util::append_allowed_request_log_fields(output, ", PARAMS: "sv, fields, allowed_names, "&"sv);

  EXPECT_EQ(output.str(), ", PARAMS: appid=123&clientname=Living room&mode=1920x1080x60??forged&uniqueid=client-01");
}

TEST(HttpUtilTest, ValidatesBrowserRequestSource) {
  struct test_case_t {
    std::optional<std::string_view> origin;
    std::optional<std::string_view> referer;
    std::optional<std::string_view> fetch_site;
    std::string_view host;
    bool allow_loopback;
    bool expected;
  };

  const std::array cases {
    // Native clients do not send browser provenance headers.
    test_case_t { std::nullopt, std::nullopt, std::nullopt, "sunshine.example:47990", false, true },
    // Browser-controlled same-origin metadata is accepted for ordinary Web UI requests.
    test_case_t { "https://sunshine.example:47990", std::nullopt, "same-origin", "sunshine.example:47990", false, true },
    // Explicit cross-site requests are rejected even when they target a valid host.
    test_case_t { "https://attacker.example", std::nullopt, "cross-site", "sunshine.example:47990", false, false },
    // Without Fetch Metadata, an HTTPS origin must match the request authority.
    test_case_t { "https://sunshine.example:47990", std::nullopt, std::nullopt, "sunshine.example:47990", false, true },
    test_case_t { "https://other.example", std::nullopt, std::nullopt, "sunshine.example:47990", false, false },
    // Loopback and Tauri origins are valid only for requests received from the local machine.
    test_case_t { "http://127.0.0.1:47990", std::nullopt, std::nullopt, "127.0.0.1:47990", true, true },
    test_case_t { "http://localhost:47990", std::nullopt, std::nullopt, "localhost:47990", false, false },
    test_case_t { "tauri://localhost", std::nullopt, std::nullopt, "tauri.localhost", true, true },
    // Invalid and ambiguous origins never pass validation.
    test_case_t { "not-an-origin", std::nullopt, std::nullopt, "sunshine.example:47990", false, false },
    test_case_t { "https://user@sunshine.example:47990", std::nullopt, std::nullopt, "sunshine.example:47990", false, false },
    test_case_t { "https://[::1", std::nullopt, std::nullopt, "[::1]:47990", true, false },
  };

  for (const auto &test : cases) {
    EXPECT_EQ(
      http_util::browser_request_source_allowed(
        test.origin,
        test.referer,
        test.fetch_site,
        test.host,
        test.allow_loopback
      ),
      test.expected
    );
  }
}
