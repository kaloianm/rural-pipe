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

#include "common/tunnel_producer_consumer.h"

#include <boost/log/trivial.hpp>
#include <chrono>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common/exception.h"
#include "common/ip_parsers.h"

namespace ruralpi {
namespace {

using Milliseconds = std::chrono::milliseconds;

const std::chrono::seconds kWaitForData(5);
const std::chrono::milliseconds kWaitForBatch(5);

void debugLogDatagram(uint8_t const *data, size_t size) {
    const auto &ip = IP::read(data);
    switch (ip.protocol) {
    case IPPROTO_ICMP:
        BOOST_LOG_TRIVIAL(debug) << "Read " << ip.tot_len << " bytes of ICMP: " << ip.toString()
                                 << ip.as<ICMP>().toString();
        break;
    case IPPROTO_TCP:
        BOOST_LOG_TRIVIAL(debug) << "Read " << ip.tot_len << " bytes of TCP: " << ip.toString()
                                 << ip.as<TCP>().toString();
        break;
    default:
        BOOST_LOG_TRIVIAL(warning)
            << "Read " << ip.tot_len << " bytes of unsupported protocol " << ip.toString();
    }
}

} // namespace

TunnelProducerConsumer::TunnelProducerConsumer(std::vector<int> tunnelFds)
    : _tunnelFds(std::move(tunnelFds)) {}

TunnelProducerConsumer::~TunnelProducerConsumer() = default;

void TunnelProducerConsumer::start() {
    assert(_receiveFromTunnelThreads.empty());

    for (int i = 0; i < _tunnelFds.size(); i++) {
        const bool isSocket = [&] {
            struct stat s;
            if (fstat(_tunnelFds[i], &s) < 0)
                Exception::throwFromErrno("Error while checking the tunnel file descriptor's type");
            return S_ISSOCK(s.st_mode);
        }();

        if (!isSocket)
            BOOST_LOG_TRIVIAL(warning) << "File descriptor is not a socket";

        int fd = _tunnelFds[i];
        BOOST_LOG_TRIVIAL(info) << "Starting thread for tunnel file descriptor " << fd;

        _receiveFromTunnelThreads.emplace_back([this, fd] {
            try {
                _receiveFromTunnelLoop(fd);

                BOOST_LOG_TRIVIAL(fatal) << "Thread for socket " << fd
                                         << " exited normally. This should never be reached.";
                BOOST_ASSERT(false);
            } catch (const std::exception &ex) {
                BOOST_LOG_TRIVIAL(info)
                    << "Thread for socket " << fd << " completed due to " << ex.what();
            }
        });
    }
}

void TunnelProducerConsumer::stop() {
    // Interrupt the producer/consumer threads and join them
    _receiveFromTunnelTasksInterrupted.store(true);

    for (auto &t : _receiveFromTunnelThreads) {
        t.join();
    }
}

void TunnelProducerConsumer::onTunnelFrameReady(TunnelFrameReader reader) {
    while (reader.next()) {
        debugLogDatagram(reader.data(), reader.size());

        // Ensure that the same source/destination address pair always uses the same tunnel device
        // queue
        const auto &ip = IP::read(reader.data());
        int tunnelFd = _tunnelFds[(ip.saddr + ip.daddr) % _tunnelFds.size()];
        int res = write(tunnelFd, reader.data(), reader.size());
        if (res < 0)
            Exception::throwFromErrno("Error writing datagram");
    }
}

void TunnelProducerConsumer::_receiveFromTunnelLoop(int tunnelFd) {
    uint8_t buffer[kTunnelFrameMaxSize];

    while (true) {
        TunnelFrameWriter writer(buffer, kTunnelFrameMaxSize);

        // Receive datagrams from the tunnel devices and write them to the frame until it is full
        int numDatagramsWritten = 0;
        while (true) {
            // Wait for the next datagram to arrive
            int res;
            while (true) {
                if (_receiveFromTunnelTasksInterrupted.load()) {
                    throw Exception("Interrupted");
                }

                BOOST_LOG_TRIVIAL(debug)
                    << "Waiting for datagrams from file descriptor " << tunnelFd << " ("
                    << numDatagramsWritten << " received so far)";

                pollfd fd;
                fd.fd = tunnelFd;
                fd.events = POLLIN;
                res = poll(
                    &fd, 1,
                    (numDatagramsWritten ? Milliseconds(kWaitForBatch) : Milliseconds(kWaitForData))
                        .count());
                if (res > 0)
                    break;
                if (res < 0)
                    Exception::throwFromErrno("Error while waiting for the next datagram");
                if (numDatagramsWritten) // res == 0
                    break;
            }

            if (res == 0) {
                assert(numDatagramsWritten);
                break;
            }

            // Check how much is the size of the next incoming datagram
            int nextDatagramSize;
            if (ioctl(tunnelFd, FIONREAD, &nextDatagramSize) < 0)
                Exception::throwFromErrno("Error while checking the next datagram size");

            if (nextDatagramSize > writer.remainingBytes()) {
                assert(numDatagramsWritten);
                break;
            }

            // Read the incoming datagram in the frame
            int nRead = read(tunnelFd, (void *)writer.data(), writer.remainingBytes());
            BOOST_LOG_TRIVIAL(debug)
                << "Received " << nRead << " bytes from tunnel socket " << tunnelFd;
            if (nRead < 0)
                Exception::throwFromErrno("Error while reading tunnel socket data");

            debugLogDatagram(writer.data(), nRead);
            writer.onDatagramWritten(nRead);

            ++numDatagramsWritten;
        }

        writer.close(_seqNum.fetch_add(1));
        _pipe->onTunnelFrameReady(TunnelFrameReader(writer.begin(), writer.totalSize()));
    }
}

} // namespace ruralpi
