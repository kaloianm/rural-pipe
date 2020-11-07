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

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>

#include "client/context.h"
#include "client/tun_ctl.h"
#include "common/ip_parsers.h"

namespace ruralpi {
namespace client {
namespace {

namespace logging = boost::log;

void initLogging() {
    logging::add_file_log(logging::keywords::file_name = "client_%N.log",
                          logging::keywords::rotation_size = 10 * 1024 * 1024,
                          logging::keywords::time_based_rotation =
                              logging::sinks::file::rotation_at_time_point(0, 0, 0),
                          logging::keywords::auto_flush = true,
                          logging::keywords::format = "[%TimeStamp%]: %Message%");

    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);

    logging::add_common_attributes();
}

void clientMain(Context ctx) {
    initLogging();

    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client starting with server " << ctx.options.serverHost
                            << ':' << ctx.options.serverPort << " and " << ctx.options.nqueues
                            << " queues";

    // Connect to the server

    TunCtl tunnel("rpi", ctx.options.nqueues);

    std::vector<std::thread> threads;
    for (int i = 0; i < ctx.options.nqueues; i++) {
        threads.emplace_back([&tunnel, queueId = i] {
            int fd = tunnel[queueId];
            constexpr int kBufferSize = 4096;
            std::string buffer(kBufferSize, 0);
            while (true) {
                int nRead = read(fd, (void *)buffer.data(), kBufferSize);
                const IP &ip = IP::read(buffer.data());
                switch (ip.protocol) {
                case IPPROTO_ICMP:
                    BOOST_LOG_TRIVIAL(debug)
                        << "Read " << nRead << " bytes of ICMP: " << ip.toString()
                        << ip.as<ICMP>().toString();
                    break;
                case IPPROTO_TCP:
                    BOOST_LOG_TRIVIAL(debug)
                        << "Read " << nRead << " bytes of TCP: " << ip.toString()
                        << ip.as<TCP>().toString();
                    break;
                default:
                    BOOST_LOG_TRIVIAL(warning)
                        << "Read " << nRead << " bytes of unsupported protocol " << ip.toString();
                }
            }
        });
    }

    std::cout << "Rural Pipe client running" << std::endl; // Required by the startup script
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client running";

    for (auto &t : threads) {
        t.join();
    }
}

} // namespace
} // namespace client
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    try {
        ruralpi::client::Context ctx(ruralpi::client::Options(argc, argv));

        if (ctx.options.help()) {
            std::cout << ctx.options.desc();
            return 1;
        }

        ruralpi::client::clientMain(std::move(ctx));
        return 0;
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred: " << ex.what();
        return 1;
    }
}
