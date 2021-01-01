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

#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <poll.h>

#include "common/exception.h"

namespace ruralpi {
namespace {

boost::uuids::basic_random_generator<boost::mt19937> uuidGen;

struct InitalExchangeResult {
    std::string identifier;
    boost::uuids::uuid sessionId;
};

InitalExchangeResult initialTunnelFrameExchange(TunnelFrameStream &stream, bool isClient) {
    uint8_t buffer[512];

    if (isClient) {
        TunnelFrameWriter writer({buffer, sizeof(buffer)});
        writer.header().sessionId = uuidGen();
        writer.header().seqNum = TunnelFrameHeader::kInitialSeqNum;
        strcpy(((char *)((InitTunnelFrame *)writer.data())->identifier), "RuralPipeClient");
        writer.onDatagramWritten(16);
        writer.close();
        stream.send(writer.buffer());

        TunnelFrameReader reader(stream.receive());
        if (!reader.next())
            throw Exception("Client received invalid initial frame response");

        return {std::string(((InitTunnelFrame const *)reader.data())->identifier),
                reader.header().sessionId};
    } else {
        TunnelFrameReader reader(stream.receive());
        if (!reader.next())
            throw Exception("Server received invalid initial frame");

        TunnelFrameWriter writer({buffer, sizeof(buffer)});
        writer.header().sessionId = reader.header().sessionId;
        writer.header().seqNum = TunnelFrameHeader::kInitialSeqNum;
        strcpy(((char *)((InitTunnelFrame *)writer.data())->identifier), "RuralPipeServer");
        writer.onDatagramWritten(16);
        writer.close();
        stream.send(writer.buffer());

        return {std::string(((InitTunnelFrame const *)reader.data())->identifier),
                reader.header().sessionId};
    }
}

} // namespace

SocketProducerConsumer::SocketProducerConsumer(bool isClient, TunnelFramePipe &pipe)
    : _isClient(isClient) {
    pipeAttach(pipe);
    BOOST_LOG_TRIVIAL(info) << "Socket producer/consumer started";
}

SocketProducerConsumer::~SocketProducerConsumer() {
    _interrupted.store(true);

    for (auto &t : _threads) {
        t.join();
    }

    pipeDetach();
    BOOST_LOG_TRIVIAL(info) << "Socket producer/consumer finished";
}

void SocketProducerConsumer::addSocket(SocketConfig config) {
    const bool isSocket = [&] {
        struct stat s;
        SYSCALL(fstat(config.fd, &s));
        return S_ISSOCK(s.st_mode);
    }();

    if (!isSocket)
        BOOST_LOG_TRIVIAL(warning) << "File descriptor " << config.fd << " is not a socket";

    BOOST_LOG_TRIVIAL(info) << "Starting thread for socket file descriptor " << config.fd;

    std::lock_guard<std::mutex> lg(_mutex);

    _streams.emplace_back(std::move(config.fd));
    _threads.emplace_back([this, &stream = _streams.back()] {
        BOOST_LOG_NAMED_SCOPE("_receiveFromSocketLoop");

        try {
            _receiveFromSocketLoop(stream);

            BOOST_LOG_TRIVIAL(info) << "Thread for socket " << stream.desc()
                                    << " exited normally. This should never be reached.";
            BOOST_ASSERT(false);
        } catch (const std::exception &ex) {
            BOOST_LOG_TRIVIAL(info)
                << "Thread for socket " << stream.desc() << " completed due to " << ex.what();
        }
    });
}

void SocketProducerConsumer::onTunnelFrameReady(TunnelFrameBuffer buf) {
    std::lock_guard<std::mutex> lg(_mutex);

    if (_streams.empty())
        throw Exception("No client connected yet");

    // TODO: Choose a client/server connection to send it on based on some bandwidth requirements
    // metric
    _streams.front().send(buf);
}

void SocketProducerConsumer::_receiveFromSocketLoop(TunnelFrameStream &stream) {
    auto ifr = initialTunnelFrameExchange(stream, _isClient);
    BOOST_LOG_TRIVIAL(info) << "Initial exchange with " << ifr.identifier << ":" << ifr.sessionId
                            << " successful";

    while (!_interrupted.load()) {
        auto buf = stream.receive();
        while (true) {
            try {
                pipeInvoke(buf);
                break;
            } catch (const NotYetReadyException &) {
                BOOST_LOG_TRIVIAL(debug) << "Not yet ready, retrying ...";
                sleep(500);
            }
        }
    }
}

TunnelFrameStream::TunnelFrameStream(ScopedFileDescriptor fd) : _fd(std::move(fd)) {}

TunnelFrameStream::~TunnelFrameStream() = default;

void TunnelFrameStream::send(TunnelFrameBuffer buf) {
    int numWritten = 0;
    while (numWritten < buf.size) {
        numWritten += _fd.write((void const *)buf.data, buf.size);
    }

    BOOST_LOG_TRIVIAL(debug) << "Written frame of " << numWritten << " bytes";
}

TunnelFrameBuffer TunnelFrameStream::receive() {
    int numRead = _fd.read((void *)_buffer, sizeof(TunnelFrameHeader));

    size_t totalSize = ((TunnelFrameHeader *)_buffer)->desc.size;
    BOOST_LOG_TRIVIAL(debug) << "Received a header of a frame of size " << totalSize;

    while (numRead < totalSize) {
        numRead += read(_fd, (void *)&_buffer[numRead], totalSize - numRead);

        BOOST_LOG_TRIVIAL(debug) << "Received " << numRead << " bytes so far";
    }

    BOOST_ASSERT(numRead == totalSize);
    return {_buffer, totalSize};
}

} // namespace ruralpi
