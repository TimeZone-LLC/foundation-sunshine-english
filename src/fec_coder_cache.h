/**
 * @file src/fec_coder_cache.h
 * @brief Per-thread cache of Reed-Solomon coders keyed by shard counts.
 * @details Building a coder allocates and fills a generator matrix. Frame sizes repeat, so the
 *          sender keeps one coder per (data, parity) pair instead of rebuilding it per block.
 */
#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <utility>

namespace stream::fec {

  /**
   * @brief Owns one coder per shard-count pair and hands out the matching one.
   * @tparam Coder The coder type.
   * @tparam Factory Callable `(int data_shards, int parity_shards) -> Coder *`.
   * @tparam Release Callable `(Coder *)` that frees a coder.
   * @note Not thread-safe by design; keep one instance per sender thread.
   */
  template <class Coder, class Factory, class Release>
  class coder_cache_t {
  public:
    /**
     * @brief Get the coder for the given shard counts, creating it on first use.
     */
    Coder *
    get(int data_shards, int parity_shards) {
      const auto key = std::make_pair(data_shards, parity_shards);
      auto it = coders.find(key);
      if (it == coders.end()) {
        it = coders.emplace(key, std::unique_ptr<Coder, Release>(Factory {}(data_shards, parity_shards))).first;
      }
      return it->second.get();
    }

    /**
     * @brief Number of coders currently held.
     */
    std::size_t
    size() const {
      return coders.size();
    }

  private:
    std::map<std::pair<int, int>, std::unique_ptr<Coder, Release>> coders;
  };

}  // namespace stream::fec
