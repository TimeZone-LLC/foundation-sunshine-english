/**
 * @file tests/unit/test_input_queue.cpp
 * @brief Regression tests for per-stream input queue scheduling.
 */

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <moonlight-common-c/src/Input.h>

#include <src/input.h>
#include <src/utility.h>

namespace {
  struct input_packet_spec_t {
    std::uint32_t magic;
    std::size_t size;
  };

  std::vector<std::uint8_t>
  make_input_packet(std::uint32_t magic, std::uint32_t declared_size, std::size_t actual_size) {
    std::vector<std::uint8_t> packet(actual_size);
    const NV_INPUT_HEADER header {
      util::endian::big(declared_size),
      util::endian::little(magic),
    };
    if (!packet.empty()) {
      std::memcpy(packet.data(), &header, std::min(packet.size(), sizeof(header)));
    }
    return packet;
  }

  std::vector<std::uint8_t>
  make_unknown_packet() {
    return make_input_packet(0x12345678, sizeof(std::uint32_t), sizeof(NV_INPUT_HEADER));
  }

  class InputQueueTest: public ::testing::Test {
  protected:
    void
    TearDown() override {
      input::testing::set_input_task_sink({});
    }
  };
}  // namespace

TEST_F(InputQueueTest, RejectsEveryBufferShorterThanTheHeader) {
  for (std::size_t actual_size = 0; actual_size < sizeof(NV_INPUT_HEADER); ++actual_size) {
    const auto packet = make_input_packet(UTF8_TEXT_EVENT_MAGIC, sizeof(std::uint32_t), actual_size);
    EXPECT_FALSE(input::testing::is_valid_input_packet(packet)) << "actual_size=" << actual_size;
  }
}

TEST_F(InputQueueTest, RejectsDeclaredSizesOutsideTheAvailableBuffer) {
  constexpr std::uint32_t unknown_magic = 0x12345678;
  for (std::uint32_t declared_size = 0; declared_size < sizeof(std::uint32_t); ++declared_size) {
    const auto packet = make_input_packet(unknown_magic, declared_size, sizeof(NV_INPUT_HEADER));
    EXPECT_FALSE(input::testing::is_valid_input_packet(packet)) << "declared_size=" << declared_size;
  }

  EXPECT_FALSE(input::testing::is_valid_input_packet(
    make_input_packet(unknown_magic, sizeof(std::uint32_t) + 1, sizeof(NV_INPUT_HEADER))));
  EXPECT_FALSE(input::testing::is_valid_input_packet(
    make_input_packet(unknown_magic, std::numeric_limits<std::uint32_t>::max(), sizeof(NV_INPUT_HEADER))));
}

TEST_F(InputQueueTest, ValidatesUnicodePacketBoundsWithoutOverflow) {
  EXPECT_TRUE(input::testing::is_valid_input_packet(
    make_input_packet(UTF8_TEXT_EVENT_MAGIC, sizeof(std::uint32_t), sizeof(NV_INPUT_HEADER))));

  constexpr std::uint32_t maximum_declared_size = sizeof(std::uint32_t) + UTF8_TEXT_EVENT_MAX_COUNT;
  EXPECT_TRUE(input::testing::is_valid_input_packet(
    make_input_packet(UTF8_TEXT_EVENT_MAGIC, maximum_declared_size, sizeof(std::uint32_t) + maximum_declared_size)));
  EXPECT_FALSE(input::testing::is_valid_input_packet(
    make_input_packet(UTF8_TEXT_EVENT_MAGIC, maximum_declared_size + 1, sizeof(std::uint32_t) + maximum_declared_size + 1)));
  EXPECT_FALSE(input::testing::is_valid_input_packet(
    make_input_packet(UTF8_TEXT_EVENT_MAGIC, maximum_declared_size, sizeof(NV_INPUT_HEADER))));
}

TEST_F(InputQueueTest, EnforcesEveryFixedPacketSize) {
  constexpr std::array packet_specs {
    input_packet_spec_t {MOUSE_MOVE_REL_MAGIC_GEN5, sizeof(NV_REL_MOUSE_MOVE_PACKET)},
    input_packet_spec_t {MOUSE_MOVE_ABS_MAGIC, sizeof(NV_ABS_MOUSE_MOVE_PACKET)},
    input_packet_spec_t {MOUSE_BUTTON_DOWN_EVENT_MAGIC_GEN5, sizeof(NV_MOUSE_BUTTON_PACKET)},
    input_packet_spec_t {MOUSE_BUTTON_UP_EVENT_MAGIC_GEN5, sizeof(NV_MOUSE_BUTTON_PACKET)},
    input_packet_spec_t {SCROLL_MAGIC_GEN5, sizeof(NV_SCROLL_PACKET)},
    input_packet_spec_t {SS_HSCROLL_MAGIC, sizeof(SS_HSCROLL_PACKET)},
    input_packet_spec_t {KEY_DOWN_EVENT_MAGIC, sizeof(NV_KEYBOARD_PACKET)},
    input_packet_spec_t {KEY_UP_EVENT_MAGIC, sizeof(NV_KEYBOARD_PACKET)},
    input_packet_spec_t {MULTI_CONTROLLER_MAGIC_GEN5, sizeof(NV_MULTI_CONTROLLER_PACKET)},
    input_packet_spec_t {SS_TOUCH_MAGIC, sizeof(SS_TOUCH_PACKET)},
    input_packet_spec_t {SS_TOUCHPAD_MAGIC, sizeof(SS_TOUCHPAD_PACKET)},
    input_packet_spec_t {SS_TOUCHPAD_FRAME_MAGIC, sizeof(SS_TOUCHPAD_FRAME_PACKET)},
    input_packet_spec_t {SS_PEN_MAGIC, sizeof(SS_PEN_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_ARRIVAL_MAGIC, sizeof(SS_CONTROLLER_ARRIVAL_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_TOUCH_MAGIC, sizeof(SS_CONTROLLER_TOUCH_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_MOTION_MAGIC, sizeof(SS_CONTROLLER_MOTION_PACKET)},
    input_packet_spec_t {SS_CONTROLLER_BATTERY_MAGIC, sizeof(SS_CONTROLLER_BATTERY_PACKET)},
  };

  for (const auto &[magic, size] : packet_specs) {
    const auto declared_size = static_cast<std::uint32_t>(size - sizeof(std::uint32_t));
    EXPECT_TRUE(input::testing::is_valid_input_packet(make_input_packet(magic, declared_size, size))) << "magic=" << magic;
    EXPECT_TRUE(input::testing::is_valid_input_packet(make_input_packet(magic, declared_size, size + 8))) << "magic=" << magic;
    EXPECT_FALSE(input::testing::is_valid_input_packet(make_input_packet(magic, declared_size - 1, size))) << "magic=" << magic;
    EXPECT_FALSE(input::testing::is_valid_input_packet(make_input_packet(magic, declared_size + 1, size + 1))) << "magic=" << magic;
    EXPECT_FALSE(input::testing::is_valid_input_packet(make_input_packet(magic, declared_size, size - 1))) << "magic=" << magic;
  }
}

TEST_F(InputQueueTest, DropsMalformedPacketsBeforeScheduling) {
  std::vector<std::shared_ptr<input::input_t>> scheduled;
  input::testing::set_input_task_sink([&](std::shared_ptr<input::input_t> stream) {
    scheduled.push_back(std::move(stream));
  });
  auto stream = input::testing::make_input();

  input::passthrough(stream, make_input_packet(SS_PEN_MAGIC, sizeof(SS_PEN_PACKET) - sizeof(std::uint32_t), sizeof(SS_PEN_PACKET) - 1));
  input::passthrough(stream, make_input_packet(KEY_DOWN_EVENT_MAGIC, sizeof(NV_KEYBOARD_PACKET) - sizeof(std::uint32_t), sizeof(NV_INPUT_HEADER)));

  EXPECT_TRUE(scheduled.empty());
  EXPECT_EQ(input::testing::queued_input_packet_count(stream), 0U);
}

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
