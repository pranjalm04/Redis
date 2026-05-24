#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr size_t k_max_msg = 4096;
constexpr size_t k_max_arg_len = 1024;

void die(const char* msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

int32_t read_full(int fd, uint8_t* buf, size_t n) {
    while (n > 0) {
        const ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        n -= static_cast<size_t>(rv);
        buf += rv;
    }
    return 0;
}

int32_t write_all(int fd, const uint8_t* buf, size_t n) {
    while (n > 0) {
        const ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        n -= static_cast<size_t>(rv);
        buf += rv;
    }
    return 0;
}

void append_u32(std::vector<uint8_t>& buf, uint32_t value) {
    const size_t off = buf.size();
    buf.resize(off + 4);
    std::memcpy(buf.data() + off, &value, 4);
}

bool read_u32(const uint8_t*& cur, const uint8_t* end, uint32_t& out) {
    if (cur + 4 > end) {
        return false;
    }
    std::memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

bool encode_request(const std::vector<std::string>& args, std::vector<uint8_t>& body) {
    if (args.empty() || args.size() > 1024) {
        return false;
    }

    body.clear();
    append_u32(body, static_cast<uint32_t>(args.size()));
    for (const std::string& arg : args) {
        if (arg.size() > k_max_arg_len) {
            return false;
        }
        append_u32(body, static_cast<uint32_t>(arg.size()));
        body.insert(body.end(), arg.begin(), arg.end());
    }
    return true;
}

int32_t send_frame(int fd, const std::vector<uint8_t>& body) {
    if (body.size() > k_max_msg) {
        return -1;
    }

    uint32_t len_be = htonl(static_cast<uint32_t>(body.size()));
    if (write_all(fd, reinterpret_cast<const uint8_t*>(&len_be), 4) != 0) {
        return -1;
    }
    if (body.empty()) {
        return 0;
    }
    return write_all(fd, body.data(), body.size());
}

int32_t recv_frame(int fd, std::vector<uint8_t>& body) {
    uint32_t len_be = 0;
    if (read_full(fd, reinterpret_cast<uint8_t*>(&len_be), 4) != 0) {
        return -1;
    }

    const uint32_t len = ntohl(len_be);
    if (len > k_max_msg) {
        return -1;
    }

    body.resize(len);
    if (len == 0) {
        return 0;
    }
    return read_full(fd, body.data(), len);
}

bool decode_response(const std::vector<uint8_t>& body) {
    const uint8_t* cur = body.data();
    const uint8_t* end = body.data() + body.size();

    uint32_t status = 0;
    if (!read_u32(cur, end, status)) {
        fprintf(stderr, "invalid response: missing status\n");
        return false;
    }

    if (cur == end) {
        if (status == 0) {
            printf("OK\n");
        } else if (status == 2) {
            printf("(nil)\n");
        } else {
            printf("status=%u\n", status);
        }
        return true;
    }

    uint32_t payload_len = 0;
    if (!read_u32(cur, end, payload_len) || cur + payload_len > end) {
        fprintf(stderr, "invalid response: bad payload length\n");
        return false;
    }

    const std::string payload(reinterpret_cast<const char*>(cur), payload_len);
    cur += payload_len;

    if (cur != end) {
        fprintf(stderr, "invalid response: trailing bytes\n");
        return false;
    }

    if (status == 0) {
        printf("%s\n", payload.c_str());
        return true;
    }
    if (status == 1) {
        fprintf(stderr, "ERR %s\n", payload.c_str());
        return true;
    }
    if (status == 2) {
        printf("(nil)\n");
        return true;
    }

    printf("status=%u payload=%s\n", status, payload.c_str());
    return true;
}

int32_t run_command(int fd, const std::vector<std::string>& args) {
    std::vector<uint8_t> request_body;
    if (!encode_request(args, request_body)) {
        fprintf(stderr, "failed to encode request\n");
        return -1;
    }

    if (send_frame(fd, request_body) != 0) {
        return -1;
    }

    std::vector<uint8_t> reply_body;
    if (recv_frame(fd, reply_body) != 0) {
        return -1;
    }

    if (!decode_response(reply_body)) {
        return -1;
    }
    return 0;
}

// Parse one input line into command arguments (lowercase command name).
bool parse_input_line(const char* line, std::vector<std::string>& args) {
    args.clear();
    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        return false;
    }

    const char* cmd_start = line;
    while (*line && *line != ' ' && *line != '\t') {
        ++line;
    }
    args.emplace_back(cmd_start, line);
    for (char& c : args.back()) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }

    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    if (*line == '\0') {
        return true;
    }

    if (args[0] == "set") {
        const char* key_start = line;
        while (*line && *line != ' ' && *line != '\t') {
            ++line;
        }
        if (key_start == line) {
            return false;
        }
        args.emplace_back(key_start, line);

        while (*line == ' ' || *line == '\t') {
            ++line;
        }
        if (*line == '\0') {
            return false;
        }
        args.emplace_back(line);
        return true;
    }

    const char* key_start = line;
    while (*line && *line != ' ' && *line != '\t') {
        ++line;
    }
    args.emplace_back(key_start, line);
    while (*line == ' ' || *line == '\t') {
        ++line;
    }
    return *line == '\0';
}

void print_usage() {
    printf("Commands:\n");
    printf("  set <key> <value>   store a key-value pair\n");
    printf("  get <key>           fetch a value\n");
    printf("  del <key>           delete a key\n");
    printf("  quit                exit\n");
}

}  // namespace

int main() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        die("connect");
    }

    print_usage();

    char line[5000];
    while (true) {
        printf("redis> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0' || strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            break;
        }

        std::vector<std::string> args;
        if (!parse_input_line(line, args)) {
            fprintf(stderr, "bad command. Try: set <key> <value> | get <key> | del <key>\n");
            continue;
        }

        if (args[0] != "set" && args[0] != "get" && args[0] != "del") {
            fprintf(stderr, "unknown command '%s'\n", args[0].c_str());
            continue;
        }
        if (args[0] == "set" && args.size() != 3) {
            fprintf(stderr, "usage: set <key> <value>\n");
            continue;
        }
        if ((args[0] == "get" || args[0] == "del") && args.size() != 2) {
            fprintf(stderr, "usage: %s <key>\n", args[0].c_str());
            continue;
        }

        if (run_command(fd, args) != 0) {
            fprintf(stderr, "request failed: %s\n", strerror(errno));
            break;
        }
    }

    close(fd);
    return 0;
}
