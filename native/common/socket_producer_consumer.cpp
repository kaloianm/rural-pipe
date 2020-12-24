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

#include "common/socket_producer_consumer.h"

#include <boost/log/trivial.hpp>
#include <chrono>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common/exception.h"

namespace ruralpi {

SocketProducerConsumer::SocketProducerConsumer(bool isClient) : _isClient(isClient) {}

SocketProducerConsumer::~SocketProducerConsumer() = default;

void SocketProducerConsumer::addSocket(SocketConfig config) {
    const bool isSocket = [&] {
        struct stat s;
        if (fstat(config.fd, &s) < 0)
            Exception::throwFromErrno("Error while checking the tunnel file descriptor's type");
        return S_ISSOCK(s.st_mode);
    }();

    if (!isSocket)
        BOOST_LOG_TRIVIAL(warning) << "File descriptor is not a socket";

    _receiveFromSocketThreads.emplace_back([this, config] {
        try {
            _receiveFromSocketLoop(config.fd);

            BOOST_LOG_TRIVIAL(info) << "Thread for socket " << config.fd
                                    << " exited normally. This should never be reached.";
            assert(false);
        } catch (const std::exception &ex) {
            BOOST_LOG_TRIVIAL(info)
                << "Thread for socket " << config.fd << " completed due to " << ex.what();
        }
    });
}

void SocketProducerConsumer::stop() {}

void SocketProducerConsumer::onTunnelFrameReady(void const *data, size_t size) {}

void SocketProducerConsumer::_receiveFromSocketLoop(int socketFd) {
    // TODO: Run the connection establishment sequence
    if (_isClient) {
    } else {
    }

    // TODO: Run the actual TunnelFrame exchange loop
}

} // namespace ruralpi
