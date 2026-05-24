#include "domain/command_dispatcher.h"

#include "storage/inmemory_cache.h"

CommandResult dispatch_command(const std::vector<std::string>& cmd, InMemoryCache& cache) {
    CommandResult result;

    if (cmd.empty()) {
        result.status = CommandStatus::Err;
        result.message = "empty command";
        return result;
    }

    if (cmd.size() == 2 && cmd[0] == "get") {
        const auto value = cache.get(cmd[1]);
        if (!value) {
            result.status = CommandStatus::NotFound;
            return result;
        }
        result.status = CommandStatus::Ok;
        result.value = *value;
        return result;
    }

    if (cmd.size() == 3 && cmd[0] == "set") {
        cache.set(cmd[1], cmd[2]);
        result.status = CommandStatus::Ok;
        return result;
    }

    if (cmd.size() == 2 && cmd[0] == "del") {
        if (!cache.del(cmd[1])) {
            result.status = CommandStatus::NotFound;
            return result;
        }
        result.status = CommandStatus::Ok;
        return result;
    }

    result.status = CommandStatus::Err;
    result.message = "unknown command";
    return result;
}
