#ifndef STORAGE_INMEMORY_CACHE_H
#define STORAGE_INMEMORY_CACHE_H

#include <cstddef>
#include <ctime>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class InMemoryCache {
  public:
    struct Entry {
        std::string key;
        std::string value;
        std::time_t timestamp{};
    };

    using EntryMap = std::unordered_map<std::string, Entry>;
    using EntryVector = std::vector<Entry>;

    explicit InMemoryCache(size_t max_size);
    ~InMemoryCache();

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    void evict(size_t count);
    size_t size() const;
    size_t max_size() const;
    void clear();

  private:
    size_t max_size_;
    EntryMap entry_map_;
    EntryVector entry_vector_;
};

#endif
