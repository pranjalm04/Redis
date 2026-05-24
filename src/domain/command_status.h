#ifndef DOMAIN_COMMAND_STATUS_H
#define DOMAIN_COMMAND_STATUS_H

#include <cstdint>

enum class CommandStatus : uint8_t {
    Ok = 0,
    Err = 1,
    NotFound = 2,  // NX — key not found
};

// Legacy names (optional); prefer CommandStatus::Ok etc.
constexpr CommandStatus RES_OK = CommandStatus::Ok;
constexpr CommandStatus RES_ERR = CommandStatus::Err;
constexpr CommandStatus RES_NX = CommandStatus::NotFound;

#endif
