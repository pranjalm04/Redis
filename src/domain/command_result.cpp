#include "domain/command_result.h"

#include <cstring>

#include "storage/inmemory_cache.h"

namespace {

constexpr const char k_err_empty_command[] = "empty command";
constexpr const char k_err_unknown_command[] = "unknown command";

size_t encoded_body_with_message(const char* message) {
    const size_t msg_len = std::strlen(message);
    return 4 + 4 + msg_len;
}

void append_u32(std::vector<uint8_t>& buf, uint32_t value) {
    buf.resize(buf.size() + 4);
    std::memcpy(buf.data() + buf.size() - 4, &value, 4);
}

}

std::vector<uint8_t> encode_command_result(const CommandResult& result) {
    std::vector<uint8_t> buf;
    append_u32(buf, static_cast<uint32_t>(result.status));

    if (result.status == CommandStatus::Ok && !result.value.empty()) {
        append_u32(buf, static_cast<uint32_t>(result.value.size()));
        buf.insert(buf.end(), result.value.begin(), result.value.end());
    } else if (!result.message.empty()) {
        append_u32(buf, static_cast<uint32_t>(result.message.size()));
        buf.insert(buf.end(), result.message.begin(), result.message.end());
    }

    return buf;
}

size_t estimate_reply_body_size(const std::vector<std::string>& cmd,
                                const InMemoryCache& cache) {
    if (cmd.empty()) {
        return encoded_body_with_message(k_err_empty_command);
    }

    if (cmd.size() == 2 && cmd[0] == "get") {
        const auto value = cache.get(cmd[1]);
        if (!value) {
            return 4;
        }
        return 4 + 4 + value->size();
    }

    if (cmd.size() == 3 && cmd[0] == "set") {
        return 4;
    }

    if (cmd.size() == 2 && cmd[0] == "del") {
        return 4;
    }

    return encoded_body_with_message(k_err_unknown_command);
}
