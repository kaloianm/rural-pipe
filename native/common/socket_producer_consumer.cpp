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

#include "common/exception.h"
#include "common/tunnel_frame_stream.h"

namespace ruralpi {
namespace {

std::string initialTunnelFrameExchange(TunnelFrameStream &stream, bool isClient) {
    uint8_t buffer[512];

    if (isClient) {
        TunnelFrameWriter writer(buffer, sizeof(buffer));
        strcpy(((char *)((InitTunnelFrame *)writer.data())->identifier), "RuralPipeClient");
        writer.onDatagramWritten(16);
        writer.close(TunnelFrameHeader::kInitialSeqNum);
        stream.send(writer);

        auto reader = stream.receive();
        if (!reader.next())
            throw Exception("Client received invalid initial frame response");

        return std::string(((InitTunnelFrame const *)reader.data())->identifier);
    } else {
        auto reader = stream.receive();
        if (!reader.next())
            throw Exception("Server received invalid initial frame");

        TunnelFrameWriter writer(buffer, sizeof(buffer));
        strcpy(((char *)((InitTunnelFrame *)writer.data())->identifier), "RuralPipeServer");
        writer.onDatagramWritten(16);
        writer.close(TunnelFrameHeader::kInitialSeqNum);
        stream.send(writer);

        return std::string(((InitTunnelFrame const *)reader.data())->identifier);
    }
}

} // namespace

SocketProducerConsumer::SocketProducerConsumer(bool isClient, TunnelFramePipe &pipe)
    : _isClient(isClient) {
    pipeTo(pipe);
}

SocketProducerConsumer::~SocketProducerConsumer() {
    for (auto &t : _receiveFromSocketThreads) {
        t.join();
    }

    unPipe();
}

void SocketProducerConsumer::addSocket(SocketConfig config) {
    const bool isSocket = [&] {
        struct stat s;
        if (fstat(config.fd, &s) < 0)
            Exception::throwFromErrno("Error while checking the tunnel file descriptor's type");
        return S_ISSOCK(s.st_mode);
    }();

    if (!isSocket)
        BOOST_LOG_TRIVIAL(warning) << "File descriptor is not a socket";

    BOOST_LOG_TRIVIAL(info) << "Starting thread for socket file descriptor " << config.fd;

    _receiveFromSocketThreads.emplace_back([this, config = std::move(config)]() mutable {
        try {
            _receiveFromSocketLoop(std::move(config));

            BOOST_LOG_TRIVIAL(info) << "Thread for socket " << config.fd
                                    << " exited normally. This should never be reached.";
            BOOST_ASSERT(false);
        } catch (const std::exception &ex) {
            BOOST_LOG_TRIVIAL(info)
                << "Thread for socket " << config.fd << " completed due to " << ex.what();
        }
    });
}

void SocketProducerConsumer::interrupt() {
    // Interrupt the producer/consumer threads and join them
    _receiveFromSocketTasksInterrupted.store(true);
}

void SocketProducerConsumer::onTunnelFrameReady(TunnelFrameReader reader) {}

void SocketProducerConsumer::_receiveFromSocketLoop(SocketConfig config) {
    TunnelFrameStream stream(std::move(config.fd));
    BOOST_LOG_TRIVIAL(info) << "Initial exchange with "
                            << initialTunnelFrameExchange(stream, _isClient) << " successful";

    while (!_receiveFromSocketTasksInterrupted.load()) {
        _pipe->onTunnelFrameReady(stream.receive());
    }
}

SocketProducerConsumer::SocketConfig::SocketConfig(std::string name_, int fd_)
    : name(std::move(name_)), fd(name, fd_) {}

} // namespace ruralpi
