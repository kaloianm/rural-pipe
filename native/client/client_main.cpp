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

#include <arpa/inet.h>
#include <boost/asio.hpp>
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

void clientMain(Context &ctx) {
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client starting with server " << ctx.serverHost << ':'
                            << ctx.serverPort << " and " << ctx.nqueues << " queues";

    // Create the client-side Tunnel device
    TunCtl tunnel("rpic", ctx.nqueues);
    TunnelProducerConsumer tunnelPC(tunnel.getQueues());
    SocketProducerConsumer socketPC(true /* isClient */, tunnelPC);

    // Connect to the server
    {
        struct sockaddr_in addr;
        {
            boost::asio::io_service io_service;
            boost::asio::ip::tcp::resolver resolver(io_service);
            boost::asio::ip::tcp::resolver::query query(ctx.serverHost,
                                                        std::to_string(ctx.serverPort));
            boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
            memcpy(&addr, iter->endpoint().data(), iter->endpoint().size());
        }

        auto addr_v4 = boost::asio::ip::address_v4(ntohl(addr.sin_addr.s_addr));
        BOOST_LOG_TRIVIAL(info) << "Connecting to " << addr_v4;

        SocketProducerConsumer::SocketConfig config(addr_v4.to_string(),
                                                    socket(AF_INET, SOCK_STREAM, 0));

        if (connect(config.fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            Exception::throwFromErrno("Failed to connect to server");

        socketPC.addSocket(std::move(config));
    }

    std::cout << "Rural Pipe client running" << std::endl; // Indicates to the startup script that
                                                           // the tunnel device has been created and
                                                           // that it can configure the routing
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client running";
}

} // namespace
} // namespace client
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    try {
        ruralpi::client::Context ctx(argc, argv);

        if (ctx.help()) {
            std::cout << ctx.desc();
            return 1;
        }

        ruralpi::client::clientMain(ctx);
        return ctx.waitForCompletion();
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred: " << ex.what();
        return 1;
    }
}
