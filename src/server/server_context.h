#ifndef SERVER_SERVER_CONTEXT_H
#define SERVER_SERVER_CONTEXT_H

#include "storage/inmemory_cache.h"

// Process-wide state shared by all client connections (e.g. the key-value store).
class ServerContext {
  public:
    explicit ServerContext(size_t max_cache_entries);

    InMemoryCache& cache();
    const InMemoryCache& cache() const;

  private:
    InMemoryCache cache_;
};

#endif
