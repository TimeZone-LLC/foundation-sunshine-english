/**
 * @file tests/unit/test_process.cpp
 * @brief Focused tests for command preparation and application-list refreshes.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "src/process.h"

namespace {
  namespace fs = std::filesystem;

  void
  write_apps_file(const fs::path &path, const std::string &app_name, const fs::file_time_type modified_at) {
    std::ofstream file(path);
    file << R"({"env":{},"apps":[{"name":")" << app_name << R"("}]})";
    file.close();
    fs::last_write_time(path, modified_at);
  }
}  // namespace

TEST(ProcessTest, PrepareCommand) {
#ifdef SUNSHINE_BUILD_FLATPAK
  EXPECT_EQ(proc::prepare_command("steam"), "flatpak-spawn --host steam");
  EXPECT_EQ(proc::prepare_command("flatpak-spawn --host steam"), "flatpak-spawn --host steam");
  EXPECT_EQ(proc::prepare_command("  flatpak-spawn --host steam  "), "flatpak-spawn --host steam");
  EXPECT_EQ(proc::prepare_command("  steam  "), "flatpak-spawn --host steam");
  EXPECT_EQ(proc::prepare_command(""), "");
  EXPECT_EQ(proc::prepare_command("  \t"), "");
#else
  EXPECT_EQ(proc::prepare_command("steam"), "steam");
  EXPECT_EQ(proc::prepare_command("  steam  "), "  steam  ");
  EXPECT_EQ(proc::prepare_command("flatpak-spawn --host steam"), "flatpak-spawn --host steam");
  EXPECT_EQ(proc::prepare_command(""), "");
#endif
}

TEST(ProcessTest, RefreshSkipsUnchangedFileAndReloadsChangedFile) {
  const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto apps_file = fs::temp_directory_path() / ("sunshine_apps_refresh_" + unique_suffix + ".json");
  const auto cleanup = util::fail_guard([&apps_file]() {
    std::error_code ec;
    fs::remove(apps_file, ec);
  });

  auto saved_proc = std::move(proc::proc);
  const auto restore_proc = util::fail_guard([&saved_proc]() {
    proc::proc = std::move(saved_proc);
  });

  const auto first_mtime = fs::file_time_type::clock::now();
  write_apps_file(apps_file, "Before edit", first_mtime);
  proc::refresh(apps_file.string());
  ASSERT_EQ(proc::proc.get_apps().size(), 1u);
  EXPECT_EQ(proc::proc.get_apps().front().name, "Before edit");

  proc::proc.get_apps().front().name = "In-memory marker";
  proc::refresh(apps_file.string());
  ASSERT_EQ(proc::proc.get_apps().size(), 1u);
  EXPECT_EQ(proc::proc.get_apps().front().name, "In-memory marker");

  write_apps_file(apps_file, "After edit", first_mtime + std::chrono::seconds { 1 });
  proc::refresh(apps_file.string());
  ASSERT_EQ(proc::proc.get_apps().size(), 1u);
  EXPECT_EQ(proc::proc.get_apps().front().name, "After edit");
}
