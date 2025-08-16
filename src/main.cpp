#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <format>
#include <iostream>
#include <string_view>

namespace ba = boost::asio;

using ba::ip::tcp;
using boost::system::error_code;

constexpr size_t THRESHOLD = 8192;

ba::awaitable<void> session(tcp::socket clientSocket, ba::io_context &ioContext) {
    try {
        std::string clientBuf;
        co_await ba::async_read_until(clientSocket, ba::dynamic_buffer(clientBuf), "\r\n\r\n", ba::use_awaitable);

        auto [host, port] = findHostPort(std::string_view(clientBuf.data(), clientBuf.size()));
        if (host.empty()) {
            clientSocket.close();
            co_return;
        }
        std::string port_s = port.empty() ? "80" : port;

        tcp::resolver resolver(ioContext);
        auto endpoints = co_await resolver.async_resolve(host, port_s, ba::use_awaitable);

        tcp::socket serverSocket(ioContext);
        co_await ba::async_connect(serverSocket, endpoints, ba::use_awaitable);

        co_await ba::async_write(serverSocket, ba::buffer(clientBuf.data(), clientBuf.size()), ba::use_awaitable);

        std::string serverBuf;
        co_await ba::async_read_until(serverSocket, ba::dynamic_buffer(serverBuf), "\r\n\r\n", ba::use_awaitable);

        co_await ba::async_write(clientSocket, ba::buffer(serverBuf.data(), serverBuf.size()), ba::use_awaitable);

        auto cl = findContentLength(std::string_view(serverBuf.data(), serverBuf.size()));
        if (cl) {
            auto pos = std::string_view(serverBuf).find("\r\n\r\n");
            size_t headerEnd = (pos == std::string_view::npos) ? serverBuf.size() : pos + 4;
            size_t already = serverBuf.size() - headerEnd;
            size_t remaining = 0;
            if (*cl > already)
                remaining = *cl - already;
            else
                remaining = 0;

            while (remaining > 0) {
                size_t want = std::min(remaining, THRESHOLD);
                std::size_t before = serverBuf.size();
                try {
                    co_await ba::async_read(serverSocket, ba::dynamic_buffer(serverBuf), ba::transfer_at_least(want),
                                            ba::use_awaitable);
                } catch (const std::exception &) {
                    // При возникновении исключения — попытаемся переслать то, что прочитали, и прервём.
                }
                std::size_t after = serverBuf.size();
                if (after > before) {
                    co_await ba::async_write(clientSocket, ba::buffer(serverBuf.data() + before, after - before),
                                             ba::use_awaitable);
                    size_t forwarded = after - before;
                    if (forwarded >= remaining)
                        remaining = 0;
                    else
                        remaining -= forwarded;
                } else {
                    break;
                }
            }
        } else {
            while (true) {
                std::size_t before = serverBuf.size();
                bool read_ok = true;
                try {
                    co_await ba::async_read(serverSocket, ba::dynamic_buffer(serverBuf), ba::transfer_at_least(1),
                                            ba::use_awaitable);
                } catch (const std::exception &) {
                    read_ok = false;
                }

                std::size_t after = serverBuf.size();
                if (after > before) {
                    co_await ba::async_write(clientSocket, ba::buffer(serverBuf.data() + before, after - before),
                                             ba::use_awaitable);
                }

                if (!read_ok)
                    break;
            }
        }

        boost::system::error_code ec1, ec2;
        clientSocket.shutdown(tcp::socket::shutdown_both, ec1);
        clientSocket.close(ec1);
    } catch (const std::exception &e) {
            clientSocket.close();
    }
    co_return;
}

class Server {
public:
    Server(ba::io_context &io_context, unsigned short port)
        : _ioContext(io_context), _acceptor(io_context, tcp::endpoint(tcp::v4(), port)), _socket(io_context) {
        do_accept();
    }

private:
    void do_accept() {
        _acceptor.async_accept(_socket, [this](error_code ec) {
            if (!ec) {
                tcp::socket clientSocket = std::move(_socket);
                _socket = tcp::socket(_ioContext);
                ba::co_spawn(_ioContext, session(std::move(clientSocket), _ioContext), ba::detached);
            }
            do_accept();
        });
    }

    ba::io_context &_ioContext;
    tcp::acceptor _acceptor;
    tcp::socket _socket;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server <listen_port>\n";
            return 1;
        }
        ba::io_context ioContext(1);

        auto port = ParsePort(argv[1]);
        if (!port) {
            std::cerr << std::format("Invalid port: '{}'. Port must be integer in range 1..65535.\n", argv[1]);
            return 2;
        }

        Server server(ioContext, *port);
        ioContext.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 3;
    }
    return 0;
}
