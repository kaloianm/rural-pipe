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
#include <netinet/ip.h>
#include <thread>

#include "client/context.h"
#include "client/tun_ctl.h"

namespace ruralpi {
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

    TunCtl tunnel("rpi", ctx.options.nqueues);

    std::vector<std::thread> threads;
    for (int i = 0; i < ctx.options.nqueues; i++) {
        threads.emplace_back([&tunnel, queueId = i] {
            int fd = tunnel[queueId];
            constexpr int kBufferSize = 4096;
            std::string buffer(kBufferSize, 0);
            while (true) {
                int nRead = read(fd, (void *)buffer.data(), kBufferSize);
                const iphdr &ip = *((iphdr *)buffer.data());
                switch (ip.protocol) {
                case IPPROTO_ICMP:
                    BOOST_LOG_TRIVIAL(debug) << "Read " << nRead << " bytes of ICMP";
                    break;
                case IPPROTO_TCP:
                    BOOST_LOG_TRIVIAL(debug) << "Read " << nRead << " bytes of TCP";
                    break;
                default:
                    BOOST_LOG_TRIVIAL(warning)
                        << "Read " << nRead << " bytes of unsupported protocol "
                        << int(ip.protocol);
                }
            }
        });
    }

    std::cout << "Rural Pipe client started with " << ctx.options.nqueues << " queues "
              << std::endl;
    BOOST_LOG_TRIVIAL(info) << "Rural Pipe client started with " << ctx.options.nqueues
                            << " queues";

    for (auto &t : threads) {
        t.join();
    }
}

} // namespace
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    try {
        ruralpi::Context ctx(ruralpi::Options(argc, argv));

        if (ctx.options.help()) {
            std::cout << ctx.options.desc();
            return 1;
        }

        ruralpi::clientMain(std::move(ctx));
        return 0;
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred: " << ex.what();
        return 1;
    }
}
