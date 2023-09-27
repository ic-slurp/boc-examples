// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#include <memory>
#include <debug/harness.h>
#include <cpp/when.h>
#include <unordered_map>
#include <optional>

using namespace verona::cpp;

namespace HashMap {
  using namespace std;

  template<typename K, typename V>
  class ShardedMap {
    using map = unordered_map<K, V>;
    size_t m_num_shards;
    vector<cown_ptr<map>> m_shards;

    size_t m_size;

    cown_ptr<map> get_shard(const K &key) {
      std::hash<K> hash_fn;
      return m_shards[hash_fn(key) & m_num_shards - 1];
    }

  public:
    ShardedMap(const size_t num_shards) : m_num_shards(num_shards), m_size(0) {
      for (size_t i = 0; i < num_shards; ++i)
        m_shards.push_back(make_cown<map>(map()));
    }

    void put(const K &key, const V &value) {
      m_size++;
      when(get_shard(key)) << [key = move(key), value = move(value)] (auto shard) mutable {
        (*shard)[move(key)] = move(value);
      };
    }

    cown_ptr<optional<V>> get(const K &key) {
      cown_ptr<optional<V>> result = make_cown<optional<V>>();
      when(get_shard(key), result) << [key = move(key)] (auto shard, auto result) mutable {
        auto it = shard->find(key);
        if (it != shard->end())
          *result = it->second;
      };
      return result;
    }

    void remove(const K &key) {
      m_size--;
      when(get_shard(key)) << [key = move(key)] (auto shard) mutable {
        shard->erase(key);
      };
    }

    size_t size() { return m_size; }

    void resize(size_t num_shards) {
      auto new_shards = vector<cown_ptr<map>>();
      for (size_t i = 0; i < num_shards; ++i)
        new_shards.push_back(make_cown<map>(map()));

      // This creates a barrier across all the old shards and new shards
      when(cown_array<map>{ &new_shards[0], new_shards.size() }, cown_array<map>{ &m_shards[0], m_shards.size() }) << [=](auto new_shards, auto shards) {
        std::hash<K> hash_fn;
        for (size_t i = 0; i < shards.length; ++i) {
          for (auto &it : *(shards.array[i])) {
            (*new_shards.array[hash_fn(it.first) & new_shards.length - 1])[it.first] = move(it.second);
          }
          shards.array[i]->clear();
        }
      };

      // The resize operation might not have completed, but pending reads/writes to new data structure
      // will queue up after the resize
      m_shards = new_shards;
    }

  };

  void run_simple()
  {
    ShardedMap<string, int> map(4);

    map.put("a", 10);

    when(map.get("a")) << [] (acquired_cown<optional<int>> result) {
      std::cout << "got:" << result->value_or(0) << std::endl;
    };

    map.put("a", 20);

    when(map.get("a")) << [] (acquired_cown<optional<int>> result) {
      std::cout << "got:" << result->value_or(0) << std::endl;
    };

    when(map.get("b")) << [] (acquired_cown<optional<int>> result) {
      std::cout << "got:" << result->value_or(0) << std::endl;
    };
  }

  void run_many_elements()
  {
    ShardedMap<string, int> map(4);

    const auto limit = 1000000;

    for (size_t i = 0; i < limit; ++i) {
      map.put(to_string(i), i);
    }

    map.put("43821", 54321);

    map.resize(16);

    map.put("43821", 12345);

    when(map.get("43821")) << [] (acquired_cown<optional<int>> result) {
      std::cout << "got:" << result->value_or(0) << std::endl;
    };
  }
};

int main(int argc, char** argv)
{
  SystematicTestHarness harness(argc, argv);
  harness.run(HashMap::run_many_elements);
}
