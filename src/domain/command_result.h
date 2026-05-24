#ifndef DOMAIN_COMMAND_RESULT_H
#define DOMAIN_COMMAND_RESULT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/command_status.h"

class InMemoryCache;

struct CommandResult {
    CommandStatus status = CommandStatus::Err;
    std::string value;
    std::string message;
};

std::vector<uint8_t> encode_command_result(const CommandResult& result);

// Bytes produced by encode_command_result for this cmd (read-only cache peek for GET).
size_t estimate_reply_body_size(const std::vector<std::string>& cmd,
                                const InMemoryCache& cache);

#endif
