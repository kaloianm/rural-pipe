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

#include <iostream>
#include <netinet/ip.h>
#include <thread>

#include "client/context.h"
#include "client/tun_ctl.h"

namespace ruralpi {
namespace {

int clientMain(Context ctx) {
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
                printf("Read %d bytes: version %d, length %d, protocol %d: \n", nRead, ip.version,
                       ip.tot_len, ip.protocol);
            }
        });
    }

    std::cout << "Rural Pipe client started with " << ctx.options.nqueues << " queues "
              << std::endl;

    for (auto &t : threads) {
        t.join();
    }

    return 0;
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
        std::cerr << "Error occurred: " << std::endl << ex.what() << std::endl;
        return 1;
    }
}
