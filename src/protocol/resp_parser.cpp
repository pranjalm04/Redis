#include "protocol/resp_parser.h"

#include <cstring>

namespace {

bool read_u32(const uint8_t*& cur, const uint8_t* end, uint32_t& out) {
    if (cur + 4 > end) {
        return false;
    }
    std::memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

bool read_str(const uint8_t*& cur, const uint8_t* end, uint32_t len, std::string& out) {
    if (len > k_max_str_len || cur + len > end) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(cur), len);
    cur += len;
    return true;
}

}  // namespace

bool parse_request(const uint8_t* data, size_t size, std::vector<std::string>& out) {
    out.clear();

    const uint8_t* cur = data;
    const uint8_t* end = data + size;

    uint32_t nstr = 0;
    if (!read_u32(cur, end, nstr)) {
        return false;
    }
    if (nstr > k_max_args) {
        return false;
    }

    out.reserve(nstr);
    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(cur, end, len)) {
            return false;
        }

        std::string s;
        if (!read_str(cur, end, len, s)) {
            return false;
        }
        out.push_back(std::move(s));
    }

    return cur == end;
}
