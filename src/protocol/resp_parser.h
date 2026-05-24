#ifndef PROTOCOL_RESP_PARSER_H
#define PROTOCOL_RESP_PARSER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr uint32_t k_max_args = 1024;
constexpr uint32_t k_max_str_len = 1024 * 1024;

// Parse one complete request body. Returns true on success; clears and fills out.
bool parse_request(const uint8_t* data, size_t size, std::vector<std::string>& out);

#endif
