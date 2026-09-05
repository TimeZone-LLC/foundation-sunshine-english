/**
 * @file tests/unit/test_thread_safe_queue.cpp
 * @brief Tests for the bounded queue used between the encoder and the sender.
 */

#include <chrono>

#include <gtest/gtest.h>

#include <src/thread_safe.h>

namespace {

  int
  pop_value(safe::queue_t<int> &queue) {
    auto value = queue.pop(std::chrono::milliseconds(50));
    EXPECT_TRUE(value.has_value());
    return value ? *value : -1;
  }

}  // namespace

TEST(SafeQueue, OverflowDropsOnlyTheOldestElement) {
  // The video packet queue must never throw away every buffered frame when the
  // sender stalls; losing the oldest one is enough to make room.
  safe::queue_t<int> queue { 3 };
  queue.raise(0);
  queue.raise(1);
  queue.raise(2);
  queue.raise(3);

  EXPECT_EQ(pop_value(queue), 1);
  EXPECT_EQ(pop_value(queue), 2);
  EXPECT_EQ(pop_value(queue), 3);
  EXPECT_FALSE(queue.peek());
}

TEST(SafeQueue, BelowCapacityKeepsEverythingInOrder) {
  safe::queue_t<int> queue { 3 };
  queue.raise(7);
  queue.raise(8);

  EXPECT_EQ(pop_value(queue), 7);
  EXPECT_EQ(pop_value(queue), 8);
}
