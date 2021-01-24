/**
 * Copyright 2020 Kaloian Manassiev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "common/base.h"

#include <boost/asio.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <sys/socket.h>

#include "common/exception.h"
#include "common/socket_producer_consumer.h"
#include "common/tun_ctl.h"
#include "common/tunnel_producer_consumer.h"
#include "server/context.h"

namespace ruralpi {
namespace server {
namespace {

namespace asio = boost::asio;

class Server {
public:
    Server(Context &ctx, SocketProducerConsumer &socketPC)
        : _ctx(ctx), _socketPC(socketPC),
          _serverSock("Server socket", ::socket(AF_INET, SOCK_STREAM, 0)) {
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(ctx.port);

        SYSCALL(::bind(_serverSock, (sockaddr *)&addr, sizeof(addr)));

        _thread = std::thread([this] {
            BOOST_LOG_NAMED_SCOPE("serverControl");
            try {
                _acceptConnection();
            } catch (const std::exception &ex) {
                BOOST_LOG_TRIVIAL(fatal) << "Server exited with error: " << ex.what();
                _ctx.exit(1);
            }
        });
    }

    ~Server() {
        if (_thread.joinable())
            _thread.join();

        BOOST_LOG_TRIVIAL(info) << "Server completed";
    }

private:
    void _acceptConnection() {
        while (true) {
            SYSCALL(::listen(_serverSock, 1));

            try {
                struct sockaddr_in addr;
                int addrlen = sizeof(addr);
                int clientSocket =
                    SYSCALL(::accept(_serverSock, (struct sockaddr *)&addr, (socklen_t *)&addrlen));

                auto addr_v4 = asio::ip::address_v4(ntohl(addr.sin_addr.s_addr));
                BOOST_LOG_TRIVIAL(info) << "Accepted connection from " << addr_v4;

                _socketPC.addSocket(SocketProducerConsumer::SocketConfig{ScopedFileDescriptor(
                    boost::str(boost::format("Client %s") % addr_v4.to_string()), clientSocket)});
            } catch (const Exception &ex) {
                BOOST_LOG_TRIVIAL(error) << ex.what();
            }
        }
    }

    Context &_ctx;
    SocketProducerConsumer &_socketPC;

    // Socket on which the server listens for connections
    ScopedFileDescriptor _serverSock;

    std::thread _thread;
};

void serverMain(Context &ctx) {
    BOOST_LOG_NAMED_SCOPE("serverMain");
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe server starting on port " << ctx.port << " with "
                            << ctx.nqueues << " queues";

    // Create the server-side Tunnel device
    TunCtl tunnel("rpis", ctx.nqueues);
    TunnelProducerConsumer tunnelPC(tunnel.getQueues(), tunnel.getMTU());
    SocketProducerConsumer socketPC(false /* isClient */, tunnelPC);
    Server server(ctx, socketPC);

    std::cout << "Rural Pipe running" << std::endl; // Indicates to the startup script that the
                                                    // tunnel device has been created and that it
                                                    // can configure the routing
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe server running";
    ctx.waitForExit();
}

} // namespace
} // namespace server
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    try {
        ruralpi::server::Context ctx;
        if (ctx.start(argc, argv) == ruralpi::server::Context::kYes)
            ruralpi::server::serverMain(ctx);

        return ctx.waitForExit();
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred: " << ex.what();
        return 1;
    }
}
