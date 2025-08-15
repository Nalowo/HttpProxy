#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <optional>

using Callback = std::function<void(std::string_view, std::string_view)>;

void iterHeaders(std::string_view req, Callback&& callback);

std::pair<std::string, std::string> findHostPort(std::string_view req);

std::optional<size_t> findContentLength(std::string_view rsp);

std::optional<unsigned short> ParsePort(std::string_view s);
