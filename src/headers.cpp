#include "headers.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>

using Callback = std::function<void(std::string_view, std::string_view)>;

namespace details {
    static std::string_view TrimView(std::string_view sv) {
        const char *ws = " \t\r\n";
        size_t start = sv.find_first_not_of(ws);
        if (start == std::string_view::npos)
            return std::string_view{};
        size_t end = sv.find_last_not_of(ws);
        return sv.substr(start, end - start + 1);
    }

    static bool IEqualsView(std::string_view a, std::string_view b) {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
    }
}

void iterHeaders(std::string_view req, Callback &&callback) {
    for (auto lineRange : req | std::views::split(std::string_view{"\r\n"})) {
        auto it = std::ranges::begin(lineRange);
        size_t len = std::ranges::distance(lineRange);
        if (len == 0) {
            break;
        }
        
        std::string_view line{&*it, len};
        auto pos = line.find(':');
        if (pos == std::string_view::npos)
            continue;

        std::string_view name = line.substr(0, pos);
        std::string_view value = line.substr(pos + 1);
        name = details::TrimView(name);
        value = details::TrimView(value);

        if (!name.empty()) {
            callback(name, value);
        }
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string host;
    std::string port;
    iterHeaders(req, [&](std::string_view name, std::string_view value) {
        if (details::IEqualsView(name, "host")) {
            auto p = value.find(':');
            if (p == std::string_view::npos) {
                host = std::string(value);
                port.clear();
            } else {
                host = std::string(value.substr(0, p));
                port = std::string(value.substr(p + 1));
            }
        }
    });
    return {host, port};
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> result;
    iterHeaders(rsp, [&](std::string_view name, std::string_view value) {
        if (details::IEqualsView(name, "content-length")) {
            size_t parsed = 0;
            const char *begin = value.data();
            const char *end = value.data() + value.size();
            auto [ptr, ec] = std::from_chars(begin, end, parsed);
            if (ec == std::errc())
                result = parsed;
        }
    });
    return result;
}
