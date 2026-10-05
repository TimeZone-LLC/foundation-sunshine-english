/**
 * @file tests/unit/test_pairing_registry.cpp
 * @brief Tests for bounded, request-scoped pairing approval state.
 */

#include <atomic>
#include <chrono>
#include <format>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "src/nvhttp.h"

namespace {
  nvhttp::pair_session_t
  pending_session(std::string unique_id, std::string name, std::string address) {
    nvhttp::pair_session_t session;
    session.client.uniqueID = std::move(unique_id);
    session.async_insert_pin.salt = "ff5dc6eda99339a8a0793e216c4257c4";
    session.async_insert_pin.device_name = std::move(name);
    session.async_insert_pin.address = std::move(address);
    session.async_insert_pin.awaiting_approval = true;
    return session;
  }

  class PairingRegistryTest: public testing::Test {
  protected:
    void SetUp() override {
      nvhttp::expire_pair_sessions(std::chrono::steady_clock::time_point::max());
    }

    void TearDown() override {
      nvhttp::expire_pair_sessions(std::chrono::steady_clock::time_point::max());
    }
  };
}  // namespace

TEST_F(PairingRegistryTest, ValidatesRestApiFields) {
  EXPECT_TRUE(nvhttp::is_valid_pairing_id(std::string(nvhttp::PAIRING_ID_SIZE, 'a')));
  EXPECT_TRUE(nvhttp::is_valid_pairing_id("0123456789ABCDEF0123456789abcdef"));
  EXPECT_FALSE(nvhttp::is_valid_pairing_id(std::string(nvhttp::PAIRING_ID_SIZE - 1, 'a')));
  EXPECT_FALSE(nvhttp::is_valid_pairing_id(std::string(nvhttp::PAIRING_ID_SIZE, 'g')));

  EXPECT_TRUE(nvhttp::is_valid_pairing_pin("0000"));
  EXPECT_FALSE(nvhttp::is_valid_pairing_pin("123"));
  EXPECT_FALSE(nvhttp::is_valid_pairing_pin("12a4"));

  EXPECT_TRUE(nvhttp::is_valid_pairing_name("Client"));
  EXPECT_TRUE(nvhttp::is_valid_pairing_name(std::string(nvhttp::MAX_PAIRING_CLIENT_NAME_SIZE, 'a')));
  EXPECT_FALSE(nvhttp::is_valid_pairing_name(""));
  EXPECT_FALSE(nvhttp::is_valid_pairing_name(std::string(nvhttp::MAX_PAIRING_CLIENT_NAME_SIZE + 1, 'a')));
}

TEST_F(PairingRegistryTest, ApprovalTargetsExplicitUnguessableId) {
  std::string first_id;
  ASSERT_EQ(
    nvhttp::insert_pair_session(pending_session("first", "First", "192.0.2.10"), first_id),
    nvhttp::pair_session_insert_e::ADDED
  );
  std::string second_id;
  ASSERT_EQ(
    nvhttp::insert_pair_session(pending_session("second", "Second", "192.0.2.20"), second_id),
    nvhttp::pair_session_insert_e::ADDED
  );
  EXPECT_EQ(first_id.size(), nvhttp::PAIRING_ID_SIZE);
  EXPECT_NE(first_id, second_id);

  // Test sessions have no parked HTTP response, so approval returns false after
  // consuming the selected request. The other request must remain untouched.
  EXPECT_FALSE(nvhttp::pin(first_id, "1234", "Approved client"));
  const auto pending = nvhttp::get_pending_pairings();
  ASSERT_EQ(pending.size(), 1);
  EXPECT_EQ(pending.front().id, second_id);
  EXPECT_EQ(pending.front().address, "192.0.2.20");
}

TEST_F(PairingRegistryTest, InvalidApprovalDoesNotConsumeARequest) {
  std::string pairing_id;
  ASSERT_EQ(
    nvhttp::insert_pair_session(pending_session("client", "Laptop", "192.0.2.10"), pairing_id),
    nvhttp::pair_session_insert_e::ADDED
  );

  EXPECT_FALSE(nvhttp::pin("not-an-id", "1234", "Laptop"));
  EXPECT_FALSE(nvhttp::pin(pairing_id, "123", "Laptop"));
  EXPECT_FALSE(nvhttp::pin(pairing_id, "1234", ""));
  ASSERT_EQ(nvhttp::get_pending_pairings().size(), 1);
}

TEST_F(PairingRegistryTest, DuplicateUniqueIdIsRejected) {
  std::string original_id;
  ASSERT_EQ(
    nvhttp::insert_pair_session(pending_session("duplicate", "Original", "192.0.2.10"), original_id),
    nvhttp::pair_session_insert_e::ADDED
  );

  std::string rejected_id;
  EXPECT_EQ(
    nvhttp::insert_pair_session(pending_session("duplicate", "Replacement", "192.0.2.20"), rejected_id),
    nvhttp::pair_session_insert_e::ALREADY_EXISTS
  );
  EXPECT_TRUE(rejected_id.empty());
  const auto pending = nvhttp::get_pending_pairings();
  ASSERT_EQ(pending.size(), 1);
  EXPECT_EQ(pending.front().id, original_id);
  EXPECT_EQ(pending.front().name, "Original");
}

TEST_F(PairingRegistryTest, StorageIsBoundedAndExpiredRequestsAreRemoved) {
  for (std::size_t index = 0; index < nvhttp::MAX_PENDING_PAIRING_SESSIONS; ++index) {
    std::string pairing_id;
    ASSERT_EQ(
      nvhttp::insert_pair_session(
        pending_session(std::format("client-{}", index), "Client", "192.0.2.10"),
        pairing_id
      ),
      nvhttp::pair_session_insert_e::ADDED
    );
  }

  std::string overflow_id;
  EXPECT_EQ(
    nvhttp::insert_pair_session(pending_session("overflow", "Overflow", "192.0.2.20"), overflow_id),
    nvhttp::pair_session_insert_e::FULL
  );
  EXPECT_EQ(nvhttp::get_pending_pairings().size(), nvhttp::MAX_PENDING_PAIRING_SESSIONS);

  nvhttp::expire_pair_sessions(std::chrono::steady_clock::time_point::max());
  EXPECT_TRUE(nvhttp::get_pending_pairings().empty());
}

TEST_F(PairingRegistryTest, CancellationRemovesOnlyTheSelectedRequest) {
  std::string first_id;
  ASSERT_EQ(
    nvhttp::insert_pair_session(pending_session("first", "First", "192.0.2.10"), first_id),
    nvhttp::pair_session_insert_e::ADDED
  );
  std::string second_id;
  ASSERT_EQ(
    nvhttp::insert_pair_session(pending_session("second", "Second", "192.0.2.20"), second_id),
    nvhttp::pair_session_insert_e::ADDED
  );

  EXPECT_TRUE(nvhttp::cancel_pairing(first_id));
  EXPECT_FALSE(nvhttp::cancel_pairing(first_id));
  const auto pending = nvhttp::get_pending_pairings();
  ASSERT_EQ(pending.size(), 1);
  EXPECT_EQ(pending.front().id, second_id);
}

TEST_F(PairingRegistryTest, ConcurrentInsertionAndCancellationRemainConsistent) {
  constexpr std::size_t session_count = 12;
  std::atomic<std::size_t> cancelled { 0 };
  std::vector<std::jthread> workers;
  workers.reserve(session_count);

  for (std::size_t index = 0; index < session_count; ++index) {
    workers.emplace_back([index, &cancelled]() {
      std::string pairing_id;
      if (nvhttp::insert_pair_session(
            pending_session(std::format("concurrent-{}", index), "Client", "192.0.2.10"),
            pairing_id
          ) == nvhttp::pair_session_insert_e::ADDED &&
          nvhttp::cancel_pairing(pairing_id)) {
        ++cancelled;
      }
    });
  }
  workers.clear();

  EXPECT_EQ(cancelled, session_count);
  EXPECT_TRUE(nvhttp::get_pending_pairings().empty());
}
