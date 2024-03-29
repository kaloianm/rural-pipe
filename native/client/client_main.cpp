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
#include <boost/uuid/random_generator.hpp>
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

boost::uuids::basic_random_generator<boost::mt19937> uuidGen;

class Client {
public:
    Client(Context &ctx, SocketProducerConsumer &socketPC)
        : _ctx(ctx), _socketPC(socketPC), _serverAddr([&ctx = _ctx] {
              struct sockaddr_in serverAddr;
              asio::io_service io_service;
              asio::ip::tcp::resolver resolver(io_service);
              asio::ip::tcp::resolver::query query(asio::ip::tcp::v4(), ctx.serverHost,
                                                   std::to_string(ctx.serverPort));
              asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
              memcpy(&serverAddr, iter->endpoint().data(), iter->endpoint().size());

              return serverAddr;
          }()) {

        BOOST_LOG_TRIVIAL(info) << "Server " << ctx.serverHost << ":" << ctx.serverPort
                                << " resolves to "
                                << asio::ip::address_v4(ntohl(_serverAddr.sin_addr.s_addr)) << ":"
                                << htons(_serverAddr.sin_port);
    }

    ~Client() { BOOST_LOG_TRIVIAL(info) << "Client completed"; }

    void runConnectToServerLoop() {
        BOOST_LOG_NAMED_SCOPE("clientControl");
        while (true) {
            try {
                for (const auto &intf : _ctx.interfaces) {
                    _socketPC.addSocket(
                        SocketProducerConsumer::SocketConfig{_connectToServer(intf)});
                }

                _ctx.waitForExit();
                break;
            } catch (const ConnRefusedSystemException &ex) {
                BOOST_LOG_TRIVIAL(trace)
                    << "Server not yet ready due to error: " << ex.what() << "; retrying ...";
                ::sleep(5);
            } catch (const Exception &ex) {
                BOOST_LOG_TRIVIAL(fatal) << "Client exited with error: " << ex.what();
                _ctx.exit(1);
                break;
            }
        }
    }

private:
    ScopedFileDescriptor _connectToServer(const std::string &interface) {
        ScopedFileDescriptor sock(boost::str(boost::format("Server on %s") % interface),
                                  ::socket(AF_INET, SOCK_STREAM, 0));

        auto localAddr = [&] {
            struct ifreq ifr = {0};
            strcpy(ifr.ifr_name, interface.c_str());
            SYSCALL(::ioctl(sock, SIOCGIFADDR, &ifr));
            RASSERT(ifr.ifr_ifru.ifru_addr.sa_family == AF_INET);
            return *((struct sockaddr_in *)&ifr.ifr_ifru.ifru_addr);
        }();

        BOOST_LOG_TRIVIAL(info) << "Address of " << interface << ": "
                                << asio::ip::address_v4(ntohl(localAddr.sin_addr.s_addr));

        SYSCALL(
            ::setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, interface.c_str(), interface.size()));

        SYSCALL(::connect(sock, (struct sockaddr *)&_serverAddr, sizeof(_serverAddr)));
        BOOST_LOG_TRIVIAL(info) << "Connected to server on interface " << interface;

        return std::move(sock);
    }

    Context &_ctx;
    SocketProducerConsumer &_socketPC;

    // Resolved server address
    const struct sockaddr_in _serverAddr;
};

void clientMain(Context &ctx) {
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client starting with server " << ctx.serverHost << ':'
                            << ctx.serverPort << " and tunnel interface " << ctx.tunnel_interface
                            << " listening on " << ctx.nqueues << " queues";

    // Create the client-side Tunnel device
    TunCtl tunnel(ctx.tunnel_interface, ctx.nqueues);
    TunnelProducerConsumer tunnelPC(tunnel.getQueues(), tunnel.getMTU());
    SocketProducerConsumer socketPC(uuidGen() /* clientSessionId */, tunnelPC);
    Client client(ctx, socketPC);

    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client running";
    ctx.signalReady();

    client.runConnectToServerLoop();
}

} // namespace
} // namespace client
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    BOOST_LOG_NAMED_SCOPE("clientMain");

    try {
        ruralpi::client::Context ctx;
        if (ctx.start(argc, argv) == ruralpi::client::Context::kYes)
            ruralpi::client::clientMain(ctx);

        return ctx.waitForExit();
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred at client startup: " << ex.what();
        return 1;
    }
}
