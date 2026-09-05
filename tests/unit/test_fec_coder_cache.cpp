/**
 * @file tests/unit/test_fec_coder_cache.cpp
 * @brief Tests for the per-thread Reed-Solomon coder cache.
 */

#include <gtest/gtest.h>

#include <src/fec_coder_cache.h>

namespace {

  struct fake_coder_t {
    int data_shards;
    int parity_shards;
  };

  int constructed = 0;
  int released = 0;

  struct fake_factory_t {
    fake_coder_t *
    operator()(int data_shards, int parity_shards) const {
      ++constructed;
      return new fake_coder_t { data_shards, parity_shards };
    }
  };

  struct fake_release_t {
    void
    operator()(fake_coder_t *coder) const {
      ++released;
      delete coder;
    }
  };

  using cache_t = stream::fec::coder_cache_t<fake_coder_t, fake_factory_t, fake_release_t>;

}  // namespace

TEST(FecCoderCache, ReusesTheCoderForTheSameShardCounts) {
  constructed = 0;
  cache_t cache;

  auto *first = cache.get(200, 10);
  auto *second = cache.get(200, 10);

  EXPECT_EQ(first, second);
  EXPECT_EQ(constructed, 1);
  EXPECT_EQ(first->data_shards, 200);
  EXPECT_EQ(first->parity_shards, 10);
}

TEST(FecCoderCache, DifferentShardCountsGetDifferentCoders) {
  constructed = 0;
  cache_t cache;

  auto *a = cache.get(200, 10);
  auto *b = cache.get(199, 10);
  auto *c = cache.get(200, 11);

  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
  EXPECT_EQ(constructed, 3);
  EXPECT_EQ(cache.size(), 3u);
}

TEST(FecCoderCache, ReleasesEveryCoderOnDestruction) {
  released = 0;
  {
    cache_t cache;
    cache.get(10, 2);
    cache.get(20, 4);
  }
  EXPECT_EQ(released, 2);
}
