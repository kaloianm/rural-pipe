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
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
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

namespace logging = boost::log;

void initLogging() {
    logging::add_file_log(logging::keywords::file_name = "server_%N.log",
                          logging::keywords::rotation_size = 10 * 1024 * 1024,
                          logging::keywords::time_based_rotation =
                              logging::sinks::file::rotation_at_time_point(0, 0, 0),
                          logging::keywords::auto_flush = true,
                          logging::keywords::format = "[%TimeStamp%]: %Message%");

    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);

    logging::add_common_attributes();
}

void serverMain(Context ctx) {
    initLogging();

    BOOST_LOG_TRIVIAL(info) << "Rural Pipe server starting on port " << ctx.options.port << " with "
                            << ctx.options.nqueues << " queues";

    // Create the server-side Tunnel device
    TunCtl tunnel("rpi", ctx.options.nqueues);
    TunnelProducerConsumer tunnelPC(tunnel.getQueues());
    SocketProducerConsumer socketPC(false /* isClient */);
    tunnelPC.pipeTo(socketPC);
    tunnelPC.start();

    // Bund to the server's listening port
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == 0)
        Exception::throwFromErrno("Failed to create socket");

    {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(ctx.options.port);

        if (bind(serverSock, (sockaddr *)&addr, sizeof(addr)) < 0)
            Exception::throwFromErrno("Failed to bind to port");
    }

    std::cout << "Rural Pipe server running" << std::endl; // Indicates to the startup script that
                                                           // the tunnel device has been created and
                                                           // that it can configure the routing
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe server running";

    // Start accepting connections from clients
    while (true) {
        if (listen(serverSock, 1) < 0)
            Exception::throwFromErrno("Failed to listen on port");

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
        ruralpi::server::Context ctx(ruralpi::server::Options(argc, argv));

        if (ctx.options.help()) {
            std::cout << ctx.options.desc();
            return 1;
        }

        ruralpi::server::serverMain(std::move(ctx));
        return 0;
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred: " << ex.what();
        return 1;
    }
}
