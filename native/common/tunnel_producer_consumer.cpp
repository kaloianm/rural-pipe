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

#include "common/tunnel_producer_consumer.h"

#include <boost/asio.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <sstream>

#include "common/exception.h"
#include "common/ip_parsers.h"

namespace ruralpi {
namespace {

const Seconds kWaitForData(5);
const Milliseconds kWaitForFullerBatch(5);

std::string debugLogDatagram(uint8_t const *data, size_t size) {
    std::stringstream ss;

    const auto &ip = IP::read(data);
    switch (ip.protocol) {
    case IPPROTO_ICMP:
        ss << "ICMP: " << ip.toString() << ip.as<ICMP>().toString();
        break;
    case IPPROTO_TCP:
        ss << "TCP: " << ip.toString() << ip.as<TCP>().toString();
        break;
    case IPPROTO_UDP:
        ss << "UDP: " << ip.toString() << ip.as<UDP>().toString();
        break;
    case SSCOPMCE::kProtoNum:
        ss << "SSCOPMCE: " << ip.toString() << ip.as<SSCOPMCE>().toString();
        break;
    default:
        ss << "UNKNOWN: " << ip.toString();
        break;
    }

    return ss.str();
}

} // namespace

TunnelProducerConsumer::TunnelProducerConsumer(std::vector<FileDescriptor> tunnelFds, int mtu)
    : TunnelFramePipe("Tunnel"), _tunnelFds(std::move(tunnelFds)), _mtu(mtu),
      _stats(_tunnelFds.size()) {
    for (int i = 0; i < _tunnelFds.size(); i++) {
        BOOST_LOG_TRIVIAL(info) << "Starting thread for tunnel file descriptor " << _tunnelFds[i];

        std::lock_guard<std::mutex> lg(_mutex);

        boost::asio::post(_pool, [this, i] {
            auto &fd = _tunnelFds[i];
            BOOST_LOG_NAMED_SCOPE("_receiveFromTunnelLoop");

            fd.makeNonBlocking();

            try {
                _receiveFromTunnelLoop(i);

                RASSERT_MSG(false, boost::format("Thread for tunnel device %s exited normally. "
                                                 "This should never be reached.") %
                                       fd.toString());
            } catch (const std::exception &ex) {
                BOOST_LOG_TRIVIAL(info) << "Thread for tunnel device " << fd.toString()
                                        << " completed due to " << ex.what();
            }
        });
    }

    BOOST_LOG_TRIVIAL(info) << "Tunnel producer/consumer started";
}

TunnelProducerConsumer::~TunnelProducerConsumer() {
    _interrupted.store(true);
    _pool.join();

    BOOST_LOG_TRIVIAL(info) << "Tunnel producer/consumer finished";
}

void TunnelProducerConsumer::onTunnelFrameFromPrev(TunnelFrameBuffer buf) {
    RASSERT_MSG(false, "Tunnel producer consumer must be the first one in the chain");
}

void TunnelProducerConsumer::onTunnelFrameFromNext(TunnelFrameBuffer buf) {
    TunnelFrameReader reader(buf);
    while (reader.next()) {
        // Ensure that the same source/destination address pair always uses the same tunnel device
        // queue
        int idxTunnelFds = ++_tunnelFdRoundRobin % _tunnelFds.size();
        auto &tunnelFd = _tunnelFds[idxTunnelFds];
        int numWritten = tunnelFd.write(reader.data(), reader.size());
        _stats.bytesOut[idxTunnelFds] += numWritten;
        BOOST_LOG_TRIVIAL(trace) << "Wrote " << numWritten << " byte datagram to tunnel socket "
                                 << tunnelFd << ": "
                                 << debugLogDatagram(reader.data(), reader.size());
    }
}

void TunnelProducerConsumer::_receiveFromTunnelLoop(int idxTunnelFds) {
    FileDescriptor &tunnelFd = _tunnelFds[idxTunnelFds];

    uint8_t buffer[kTunnelFrameMaxSize];
    uint8_t *mtuBuffer = (uint8_t *)alloca(_mtu);
    int mtuBufferSize = 0;

    memset(buffer, 0xAA, sizeof(buffer));
    memset(mtuBuffer, 0xBB, _mtu);

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

                BOOST_LOG_TRIVIAL(trace)
                    << "Waiting for datagrams from file descriptor " << tunnelFd << " ("
                    << numDatagramsWritten << " datagrams received so far)";

                res = tunnelFd.poll(numDatagramsWritten ? Milliseconds(kWaitForFullerBatch)
                                                        : Milliseconds(kWaitForData),
                                    POLLIN);
                if (res > 0)
                    break;
                if (numDatagramsWritten) // res == 0
                    break;
            }

            // Nothing was received for some time, see whether we managed to batch some frames in
            // the buffer, in which case they should be sent
            if (res == 0) {
                RASSERT(numDatagramsWritten);
                break;
            }

            RASSERT(res == 1);

            // Read the incoming datagram in the MTU buffer
            if (!mtuBufferSize) {
                mtuBufferSize = tunnelFd.read(mtuBuffer, _mtu);
            }

            if (mtuBufferSize > writer.remainingBytes()) {
                RASSERT(numDatagramsWritten);
                break;
            }

            memcpy(writer.data(), mtuBuffer, mtuBufferSize);
            _stats.bytesIn[idxTunnelFds] += mtuBufferSize;
            BOOST_LOG_TRIVIAL(trace)
                << "Received " << mtuBufferSize << " byte datagram from tunnel socket " << tunnelFd
                << ": " << debugLogDatagram(writer.data(), mtuBufferSize);
            writer.onDatagramWritten(mtuBufferSize);
            mtuBufferSize = 0;

            ++numDatagramsWritten;
        }

        writer.header().seqNum = _seqNum.fetch_add(1);
        writer.close();

        while (true) {
            if (_interrupted.load()) {
                throw Exception("Interrupted");
            }

            try {
                pipeInvokeNext(writer.buffer());
                break;
            } catch (const NotYetReadyException &ex) {
                BOOST_LOG_TRIVIAL(trace)
                    << "Socket not yet ready: " << ex.what() << "; retrying ...";
                ::sleep(5);
            }
        }
    }
}

} // namespace ruralpi
