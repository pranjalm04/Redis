#include "server/server_context.h"

ServerContext::ServerContext(size_t max_cache_entries) : cache_(max_cache_entries) {}

InMemoryCache& ServerContext::cache() { return cache_; }

const InMemoryCache& ServerContext::cache() const { return cache_; }
