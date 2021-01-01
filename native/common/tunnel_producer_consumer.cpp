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

#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <poll.h>
#include <sys/ioctl.h>

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
        BOOST_LOG_TRIVIAL(debug) << "ICMP: " << ip.toString() << ip.as<ICMP>().toString();
        break;
    case IPPROTO_TCP:
        BOOST_LOG_TRIVIAL(debug) << "TCP: " << ip.toString() << ip.as<TCP>().toString();
        break;
    case IPPROTO_UDP:
        BOOST_LOG_TRIVIAL(debug) << "UDP: " << ip.toString() << ip.as<UDP>().toString();
        break;
    case SSCOPMCE::kProtoNum:
        BOOST_LOG_TRIVIAL(debug) << "SSCOPMCE: " << ip.toString() << ip.as<SSCOPMCE>().toString();
        break;
    default:
        BOOST_LOG_TRIVIAL(warning) << "UNKNOWN: " << ip.toString();
        break;
    }
}

} // namespace

TunnelProducerConsumer::TunnelProducerConsumer(std::vector<FileDescriptor> tunnelFds, int mtu)
    : _tunnelFds(std::move(tunnelFds)), _mtu(mtu) {
    for (auto &fd : _tunnelFds) {
        const bool isSocket = [&] {
            struct stat s;
            SYSCALL(fstat(fd, &s));
            return S_ISSOCK(s.st_mode);
        }();

        if (!isSocket)
            BOOST_LOG_TRIVIAL(warning) << "File descriptor " << fd << " is not a socket";

        BOOST_LOG_TRIVIAL(info) << "Starting thread for tunnel file descriptor " << fd;

        std::lock_guard<std::mutex> lg(_mutex);

        _threads.emplace_back([this, &fd] {
            BOOST_LOG_NAMED_SCOPE("_receiveFromTunnelLoop");

            try {
                _receiveFromTunnelLoop(fd);

                BOOST_LOG_TRIVIAL(info) << "Thread for tunnel device " << fd.desc()
                                        << " exited normally. This should never be reached.";
                BOOST_ASSERT(false);
            } catch (const std::exception &ex) {
                BOOST_LOG_TRIVIAL(info) << "Thread for tunnel device " << fd.desc()
                                        << " completed due to " << ex.what();
            }
        });
    }

    BOOST_LOG_TRIVIAL(info) << "Tunnel producer/consumer started";
}

TunnelProducerConsumer::~TunnelProducerConsumer() {
    _interrupted.store(true);

    for (auto &t : _threads) {
        t.join();
    }

    BOOST_LOG_TRIVIAL(info) << "Tunnel producer/consumer finished";
}

void TunnelProducerConsumer::onTunnelFrameReady(TunnelFrameBuffer buf) {
    TunnelFrameReader reader(buf);
    while (reader.next()) {
        debugLogDatagram(reader.data(), reader.size());

        // Ensure that the same source/destination address pair always uses the same tunnel device
        // queue
        const auto &ip = IP::read(reader.data());
        auto &tunnelFd = _tunnelFds[(ip.saddr + ip.daddr) % _tunnelFds.size()];
        tunnelFd.write(reader.data(), reader.size());
    }
}

void TunnelProducerConsumer::_receiveFromTunnelLoop(FileDescriptor &tunnelFd) {
    uint8_t buffer[kTunnelFrameMaxSize];
    uint8_t *mtu = (uint8_t *)alloca(_mtu);
    int mtuBufferSize = 0;

    memset(buffer, 0xAA, sizeof(buffer));
    memset(mtu, 0xBB, _mtu);

    while (true) {
        TunnelFrameWriter writer({buffer, kTunnelFrameMaxSize});

        // Receive datagrams from the tunnel devices and write them to the frame until it is full
        int numDatagramsWritten = 0;
        while (true) {
            // Wait for the next datagram to arrive
            int res;
            while (true) {
                if (_interrupted.load()) {
                    throw Exception("Interrupted");
                }

                if (mtuBufferSize) {
                    res = 1;
                    break;
                }

                BOOST_LOG_TRIVIAL(debug)
                    << "Waiting for datagrams from file descriptor " << tunnelFd << " ("
                    << numDatagramsWritten << " received so far)";

                pollfd fd;
                fd.fd = tunnelFd;
                fd.events = POLLIN;
                res = SYSCALL(poll(
                    &fd, 1,
                    (numDatagramsWritten ? Milliseconds(kWaitForBatch) : Milliseconds(kWaitForData))
                        .count()));
                if (res > 0)
                    break;
                if (numDatagramsWritten) // res == 0
                    break;
            }

            // Nothing was received for some time, see whether we managed to batch some frames in
            // the buffer, in which case they should be sent
            if (res == 0) {
                BOOST_ASSERT(numDatagramsWritten);
                break;
            }

            BOOST_ASSERT(res == 1);

            // Read the incoming datagram in the MTU buffer
            if (!mtuBufferSize) {
                mtuBufferSize = tunnelFd.read(mtu, _mtu);
                BOOST_LOG_TRIVIAL(debug)
                    << "Received " << mtuBufferSize << " bytes from tunnel socket " << tunnelFd;
            }

            if (mtuBufferSize > writer.remainingBytes()) {
                BOOST_ASSERT(numDatagramsWritten);
                break;
            }

            memcpy(writer.data(), mtu, mtuBufferSize);
            debugLogDatagram(writer.data(), mtuBufferSize);
            writer.onDatagramWritten(mtuBufferSize);
            mtuBufferSize = 0;

            ++numDatagramsWritten;
        }

        writer.header().seqNum = _seqNum.fetch_add(1);
        writer.close();

        while (true) {
            try {
                pipeInvoke(writer.buffer());
                break;
            } catch (const NotYetReadyException &ex) {
                BOOST_LOG_TRIVIAL(debug)
                    << "Client/server not yet ready: " << ex.what() << "; retrying ...";
                sleep(1);
            }
        }
    }
}

} // namespace ruralpi
