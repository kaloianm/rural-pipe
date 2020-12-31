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

#include <boost/asio/ip/address.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>

#include "common/exception.h"
#include "common/socket_producer_consumer.h"
#include "common/tun_ctl.h"
#include "common/tunnel_producer_consumer.h"
#include "server/context.h"

namespace ruralpi {
namespace server {
namespace {

void serverMain(Context &ctx) {
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe server starting on port " << ctx.port << " with "
                            << ctx.nqueues << " queues";

    // Create the server-side Tunnel device
    TunCtl tunnel("rpis", ctx.nqueues);
    TunnelProducerConsumer tunnelPC(tunnel.getQueues());
    SocketProducerConsumer socketPC(false /* isClient */, tunnelPC);

    // Bind to the server's listening port
    ScopedFileDescriptor serverSock("Server main socket", socket(AF_INET, SOCK_STREAM, 0));
    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(ctx.port);

        if (bind(serverSock, (sockaddr *)&addr, sizeof(addr)) < 0)
            SystemException::throwFromErrno("Failed to bind to port");
    }

    std::cout << "Rural Pipe server running" << std::endl; // Indicates to the startup script that
                                                           // the tunnel device has been created and
                                                           // that it can configure the routing
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe server running";

    // Start accepting connections from clients
    while (true) {
        if (listen(serverSock, 1) < 0)
            SystemException::throwFromErrno("Failed to listen on port");

        sockaddr_in addr;
        int addrlen;
        int clientSocket = accept(serverSock, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
        if (clientSocket < 0) {
            BOOST_LOG_TRIVIAL(info) << "Failed to accept connection";
            continue;
        }

        auto addr_v4 = boost::asio::ip::address_v4(ntohl(addr.sin_addr.s_addr));
        BOOST_LOG_TRIVIAL(info) << "Accepted connection from " << addr_v4;

        socketPC.addSocket(SocketProducerConsumer::SocketConfig{addr_v4.to_string(), clientSocket});
    }
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
