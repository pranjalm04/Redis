#ifndef DOMAIN_COMMAND_DISPATCHER_H
#define DOMAIN_COMMAND_DISPATCHER_H

#include <string>
#include <vector>

#include "domain/command_result.h"

class InMemoryCache;

CommandResult dispatch_command(const std::vector<std::string>& cmd, InMemoryCache& cache);

#endif
