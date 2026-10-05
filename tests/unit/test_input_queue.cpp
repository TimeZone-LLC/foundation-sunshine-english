/**
 * @file tests/unit/test_input_queue.cpp
 * @brief Regression tests for per-stream input queue scheduling.
 */

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <moonlight-common-c/src/Input.h>

#include <src/input.h>
#include <src/utility.h>

namespace {
  std::vector<std::uint8_t>
  make_unknown_packet() {
    NV_INPUT_HEADER header {};
    header.size = util::endian::big<std::uint32_t>(sizeof(header) - sizeof(header.size));
    header.magic = util::endian::little<std::uint32_t>(0x12345678);

    std::vector<std::uint8_t> packet(sizeof(header));
    std::memcpy(packet.data(), &header, sizeof(header));
    return packet;
  }

  class InputQueueTest: public ::testing::Test {
  protected:
    void
    TearDown() override {
      input::testing::set_input_task_sink({});
    }
  };
}  // namespace

TEST_F(InputQueueTest, SchedulesOnlyOneWorkerPerStream) {
  std::vector<std::shared_ptr<input::input_t>> scheduled;
  input::testing::set_input_task_sink([&](std::shared_ptr<input::input_t> stream) {
    scheduled.push_back(std::move(stream));
  });

  auto first = input::testing::make_input();
  auto second = input::testing::make_input();
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  for (int packet = 0; packet < 3; ++packet) {
    input::passthrough(first, make_unknown_packet());
  }
  EXPECT_EQ(scheduled.size(), 1U);
  EXPECT_EQ(input::testing::queued_input_packet_count(first), 3U);

  input::passthrough(second, make_unknown_packet());
  ASSERT_EQ(scheduled.size(), 2U);
  EXPECT_EQ(scheduled.back(), second);

  input::testing::process_queued_messages(first);
  EXPECT_EQ(input::testing::queued_input_packet_count(first), 0U);
  EXPECT_EQ(scheduled.size(), 2U);

  input::passthrough(first, make_unknown_packet());
  ASSERT_EQ(scheduled.size(), 3U);
  EXPECT_EQ(scheduled.back(), first);
}

TEST_F(InputQueueTest, YieldsAfterBoundedPacketBatch) {
  std::vector<std::shared_ptr<input::input_t>> scheduled;
  input::testing::set_input_task_sink([&](std::shared_ptr<input::input_t> stream) {
    scheduled.push_back(std::move(stream));
  });

  auto stream = input::testing::make_input();
  ASSERT_NE(stream, nullptr);

  constexpr std::size_t packet_count = 33;
  for (std::size_t packet = 0; packet < packet_count; ++packet) {
    input::passthrough(stream, make_unknown_packet());
  }
  ASSERT_EQ(scheduled.size(), 1U);

  input::testing::process_queued_messages(scheduled.front());
  EXPECT_EQ(input::testing::queued_input_packet_count(stream), 1U);
  ASSERT_EQ(scheduled.size(), 2U);

  input::testing::process_queued_messages(scheduled.back());
  EXPECT_EQ(input::testing::queued_input_packet_count(stream), 0U);
  EXPECT_EQ(scheduled.size(), 2U);
}
