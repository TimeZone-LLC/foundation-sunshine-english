#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <gtest/gtest.h>

#include "src/config.h"

namespace {
  class temporary_clients_config_t {
  public:
    temporary_clients_config_t():
        original_path_(config::sunshine.config_file),
        original_clients_(config::get_clients_config()),
        path_(
          std::filesystem::temp_directory_path() /
          ("sunshine_clients_config_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".conf")
        ) {
      config::sunshine.config_file = path_.string();
      std::ofstream(path_, std::ios::binary | std::ios::trunc) << "sunshine_name = retained\n";
    }

    ~temporary_clients_config_t() {
      config::sunshine.config_file = original_path_;
      config::nvhttp.clients = original_clients_;
      std::error_code ignored;
      std::filesystem::remove(path_, ignored);
    }

    std::string read() const {
      std::ifstream file(path_, std::ios::binary);
      return {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
      };
    }

  private:
    std::string original_path_;
    std::string original_clients_;
    std::filesystem::path path_;
  };
}  // namespace

using NvencPresetNameParam = std::pair<int, std::string_view>;

struct NvencPresetNameTest: testing::TestWithParam<NvencPresetNameParam> {};

TEST_P(NvencPresetNameTest, UsesStableSymbolicName) {
  const auto &[quality_preset, expected] = GetParam();
  EXPECT_EQ(expected, config::nv::ffmpeg_preset_from_quality(quality_preset));
}

INSTANTIATE_TEST_SUITE_P(
  NvencQualityPresets,
  NvencPresetNameTest,
  testing::Values(
    NvencPresetNameParam {1, "p1"},
    NvencPresetNameParam {2, "p2"},
    NvencPresetNameParam {3, "p3"},
    NvencPresetNameParam {4, "p4"},
    NvencPresetNameParam {5, "p5"},
    NvencPresetNameParam {6, "p6"},
    NvencPresetNameParam {7, "p7"}
  )
);

TEST(AmdMaxAuSizeConfig, AcceptsOnlyTheSupportedRange) {
  EXPECT_FALSE(config::amd::is_valid_max_au_size(-2));
  EXPECT_TRUE(config::amd::is_valid_max_au_size(-1));
  EXPECT_TRUE(config::amd::is_valid_max_au_size(0));
  EXPECT_TRUE(config::amd::is_valid_max_au_size(6'400'000));
  EXPECT_TRUE(config::amd::is_valid_max_au_size(std::numeric_limits<int>::max()));
}

TEST(VaapiConfig, ParsesQualityAndRateControlWithoutStickingOnAutomatic) {
  EXPECT_EQ(config::vaapi::quality_from_view("speed", 0), 1);
  EXPECT_EQ(config::vaapi::quality_from_view("balanced", 0), 2);
  EXPECT_EQ(config::vaapi::quality_from_view("quality", 0), 3);
  EXPECT_EQ(config::vaapi::quality_from_view("auto", 3), 0);
  EXPECT_EQ(config::vaapi::quality_from_view("invalid", 2), 2);

  EXPECT_EQ(config::vaapi::rate_control_from_view("cbr", 0), 0x00000002);
  EXPECT_EQ(config::vaapi::rate_control_from_view("vbr", 0), 0x00000004);
  EXPECT_EQ(config::vaapi::rate_control_from_view("qvbr", 0), 0x00000400);
  EXPECT_EQ(config::vaapi::rate_control_from_view("avbr", 0), 0x00000800);
  EXPECT_EQ(config::vaapi::rate_control_from_view("auto", 0x00000004), 0);
  EXPECT_EQ(config::vaapi::rate_control_from_view("invalid", 0x00000002), 0x00000002);
}

TEST(ConfigParse, PreservesHashCharactersInsideJsonStrings) {
  const auto parsed = config::parse_config(
    R"(clients = [{"uuid":"client-1","name":"Display #1 ]","hdrProfile":"Custom#.icc"}] # trailing comment
encoder = nvenc# ordinary comment
)"
  );

  ASSERT_TRUE(parsed.contains("clients"));
  EXPECT_EQ(parsed.at("clients"), R"([{"uuid":"client-1","name":"Display #1 ]","hdrProfile":"Custom#.icc"}])");
  ASSERT_TRUE(parsed.contains("encoder"));
  EXPECT_EQ(parsed.at("encoder"), "nvenc");
}

TEST(ConfigParse, ReadsStopOnLastVideoSession) {
  const auto enabled = config::parse_config("stop_on_last_video_session = enabled\n");
  const auto disabled = config::parse_config("stop_on_last_video_session = disabled\n");
  const auto missing = config::parse_config("ping_timeout = 10000\n");

  ASSERT_TRUE(enabled.contains("stop_on_last_video_session"));
  EXPECT_EQ(enabled.at("stop_on_last_video_session"), "enabled");
  ASSERT_TRUE(disabled.contains("stop_on_last_video_session"));
  EXPECT_EQ(disabled.at("stop_on_last_video_session"), "disabled");
  EXPECT_FALSE(missing.contains("stop_on_last_video_session"));
}

TEST(ConfigParse, ClientSettingsAreNormalizedBeforePersistence) {
  temporary_clients_config_t temporary_config;
  constexpr auto expected = R"([{"name":"Display #1","uuid":"client-1"}])";

  ASSERT_TRUE(config::save_clients_config(
    R"(
      [{"uuid":"client-1", "name":"Display #1"}]
    )"
  ));

  const auto persisted = config::parse_config(temporary_config.read());
  ASSERT_TRUE(persisted.contains("clients"));
  EXPECT_EQ(persisted.at("clients"), expected);
  EXPECT_EQ(persisted.at("sunshine_name"), "retained");
  EXPECT_EQ(config::get_clients_config(), expected);
}
