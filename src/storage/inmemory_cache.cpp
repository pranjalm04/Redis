#include "storage/inmemory_cache.h"

#include <ctime>

InMemoryCache::InMemoryCache(size_t max_size) : max_size_(max_size) {}

InMemoryCache::~InMemoryCache() { clear(); }

void InMemoryCache::set(const std::string& key, const std::string& value) {
    const auto it = entry_map_.find(key);
    if (it != entry_map_.end()) {
        it->second.value = value;
        it->second.timestamp = std::time(nullptr);
        return;
    }

    if (entry_vector_.size() >= max_size_) {
        evict(1);
    }

    entry_map_[key] = Entry{key, value, std::time(nullptr)};
    entry_vector_.push_back(entry_map_[key]);
}

bool InMemoryCache::del(const std::string& key) {
    const auto it = entry_map_.find(key);
    if (it == entry_map_.end()) {
        return false;
    }
    entry_map_.erase(it);
    for (auto vit = entry_vector_.begin(); vit != entry_vector_.end(); ++vit) {
        if (vit->key == key) {
            entry_vector_.erase(vit);
            break;
        }
    }
    return true;
}

std::optional<std::string> InMemoryCache::get(const std::string& key) const {
    const auto it = entry_map_.find(key);
    if (it == entry_map_.end()) {
        return std::nullopt;
    }
    return it->second.value;
}

void InMemoryCache::evict(size_t count) {
    while (count > 0 && !entry_vector_.empty()) {
        entry_map_.erase(entry_vector_.front().key);
        entry_vector_.erase(entry_vector_.begin());
        --count;
    }
}

size_t InMemoryCache::size() const { return entry_vector_.size(); }

size_t InMemoryCache::max_size() const { return max_size_; }

void InMemoryCache::clear() {
    entry_map_.clear();
    entry_vector_.clear();
}
