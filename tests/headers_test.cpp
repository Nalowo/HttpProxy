#include <gtest/gtest.h>
#include "headers.h"

TEST(iterHeaders, Empty) {
    std::string headers;
    int count = 0;
    iterHeaders(headers, [&](std::string_view, std::string_view) {
        count++;
    });
    EXPECT_EQ(count, 0);
}

TEST(iterHeaders, SkipRequestLine) {
    std::string headers = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    std::vector<std::pair<std::string, std::string>> result;
    iterHeaders(headers, [&](std::string_view key, std::string_view value) {
        result.emplace_back(key, value);
    });
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first, "Host");
    EXPECT_EQ(result[0].second, "example.com");
}

TEST(iterHeaders, SingleHeader) {
    std::string headers = "GET / HTTP/1.1\r\nContent-Type: text/html\r\n\r\n";
    std::vector<std::pair<std::string, std::string>> result;
    iterHeaders(headers, [&](std::string_view key, std::string_view value) {
        result.emplace_back(key, value);
    });
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first, "Content-Type");
    EXPECT_EQ(result[0].second, "text/html");
}

TEST(iterHeaders, MultipleHeaders) {
    std::string headers = "GET / HTTP/1.1\r\nHost: example.com\r\nContent-Length: 100\r\n\r\n";
    std::vector<std::pair<std::string, std::string>> result;
    iterHeaders(headers, [&](std::string_view key, std::string_view value) {
        result.emplace_back(key, value);
    });
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].first, "Host");
    EXPECT_EQ(result[0].second, "example.com");
    EXPECT_EQ(result[1].first, "Content-Length");
    EXPECT_EQ(result[1].second, "100");
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::string headers = "GET / HTTP/1.1\r\nSet-Cookie: a=1\r\nSet-Cookie: b=2\r\n\r\n";
    std::vector<std::pair<std::string, std::string>> result;
    iterHeaders(headers, [&](std::string_view key, std::string_view value) {
        result.emplace_back(key, value);
    });
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].first, "Set-Cookie");
    EXPECT_EQ(result[0].second, "a=1");
    EXPECT_EQ(result[1].first, "Set-Cookie");
    EXPECT_EQ(result[1].second, "b=2");
}

TEST(findHostPort, Simple) {
    std::string headers = "GET / HTTP/1.1\r\nHost: example.com:8080\r\n\r\n";
    auto [host, port] = findHostPort(headers);
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "8080");
}

TEST(findHostPort, NoHost) {
    std::string headers = "GET / HTTP/1.1\r\nContent-Type: text/html\r\n\r\n";
    auto [host, port] = findHostPort(headers);
    EXPECT_TRUE(host.empty());
    EXPECT_TRUE(port.empty());
}

TEST(findHostPort, BrokenPortString) {
    std::string headers = "GET / HTTP/1.1\r\nHost: example.com:unreadable\r\n\r\n";
    auto [host, port] = findHostPort(headers);
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "unreadable"); // текущая реализация возвращает всё после ':' как port
}

TEST(findContentLength, Simple) {
    std::string headers = "HTTP/1.1 200 OK\r\nContent-Length: 1024\r\n\r\n";
    auto content_length = findContentLength(headers);
    ASSERT_TRUE(content_length.has_value());
    EXPECT_EQ(*content_length, 1024);
}

TEST(findContentLength, NoContentLength) {
    std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    auto content_length = findContentLength(headers);
    EXPECT_FALSE(content_length.has_value());
}

TEST(findContentLength, TooLargeIgnored) {
    std::string headers = "HTTP/1.1 200 OK\r\nContent-Length: 1000000000000\r\n\r\n"; // 1e12 > 1e9 limit
    auto content_length = findContentLength(headers);
    EXPECT_FALSE(content_length.has_value());
}
