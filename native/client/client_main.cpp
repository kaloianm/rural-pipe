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

#include <boost/asio.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <sys/socket.h>

#include "client/context.h"
#include "common/exception.h"
#include "common/socket_producer_consumer.h"
#include "common/tun_ctl.h"
#include "common/tunnel_producer_consumer.h"

namespace ruralpi {
namespace client {
namespace {

namespace asio = boost::asio;

class Client {
public:
    Client(Context &ctx, SocketProducerConsumer &socketPC) : _socketPC(socketPC) {
        _socketPC.addSocket(
            SocketProducerConsumer::SocketConfig{_connectTo(ctx.serverHost, ctx.serverPort)});
    }

private:
    static ScopedFileDescriptor _connectTo(const std::string &serverHost, int serverPort) {
        sockaddr_in addr;

        {
            asio::io_service io_service;
            asio::ip::tcp::resolver resolver(io_service);
            asio::ip::tcp::resolver::query query(serverHost, std::to_string(serverPort));
            asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
            memcpy(&addr, iter->endpoint().data(), iter->endpoint().size());
        }

        auto addr_v4 = asio::ip::address_v4(ntohl(addr.sin_addr.s_addr));
        BOOST_LOG_TRIVIAL(info) << "Connecting to " << addr_v4;

        ScopedFileDescriptor sock(addr_v4.to_string(), ::socket(AF_INET, SOCK_STREAM, 0));
        SYSCALL(::connect(sock, (struct sockaddr *)&addr, sizeof(addr)));

        return std::move(sock);
    }

    SocketProducerConsumer &_socketPC;
};

void clientMain(Context &ctx) {
    BOOST_LOG_NAMED_SCOPE("clientMain");
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client starting with server " << ctx.serverHost << ':'
                            << ctx.serverPort << " and " << ctx.nqueues << " queues";

    // Create the client-side Tunnel device
    TunCtl tunnel("rpic", ctx.nqueues);
    TunnelProducerConsumer tunnelPC(tunnel.getQueues(), tunnel.getMTU());
    SocketProducerConsumer socketPC(true /* isClient */, tunnelPC);
    Client client(ctx, socketPC);

    std::cout << "Rural Pipe client running" << std::endl; // Indicates to the startup script that
                                                           // the tunnel device has been created and
                                                           // that it can configure the routing
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client running";
    ctx.waitForExit();
}

} // namespace
} // namespace client
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    try {
        ruralpi::client::Context ctx;
        if (ctx.start(argc, argv) == ruralpi::client::Context::kYes)
            ruralpi::client::clientMain(ctx);

        return ctx.waitForExit();
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred: " << ex.what();
        return 1;
    }
}
