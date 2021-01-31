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

#include "common/socket_producer_consumer.h"

#include <boost/asio.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "common/exception.h"

namespace ruralpi {
namespace {

boost::uuids::basic_random_generator<boost::mt19937> uuidGen;

struct InitialExchangeResult {
    std::string identifier;
    SessionId sessionId;
};

InitialExchangeResult initialTunnelFrameExchange(TunnelFrameStream &stream, bool isClient) {
    uint8_t buffer[1024];
    memset(buffer, 0xAA, sizeof(buffer));

    if (isClient) {
        TunnelFrameWriter writer({buffer, sizeof(buffer)});

        // The client generates the session, the server just accepts it
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

SocketProducerConsumer::SocketProducerConsumer(bool isClient, TunnelFramePipe &prev)
    : TunnelFramePipe("Socket"), _isClient(isClient) {
    _compresser.emplace(prev);
    _signer.emplace(*_compresser);
    pipePush(*_signer);
    BOOST_LOG_TRIVIAL(info) << "Socket producer/consumer started";
}

SocketProducerConsumer::~SocketProducerConsumer() {
    _interrupted.store(true);
    _pool.join();

    RASSERT(_sessions.empty());

    pipePop();
    _signer.reset();
    _compresser.reset();
    BOOST_LOG_TRIVIAL(info) << "Socket producer/consumer finished";
}

void SocketProducerConsumer::addSocket(SocketConfig config) {
    const bool isSocket = [&] {
        struct stat s;
        SYSCALL(::fstat(config.fd, &s));
        return S_ISSOCK(s.st_mode);
    }();

    if (!isSocket)
        BOOST_LOG_TRIVIAL(warning)
            << "File descriptor " << config.fd.toString() << " is not a socket";

    BOOST_LOG_TRIVIAL(info) << "Starting thread for socket file descriptor "
                            << config.fd.toString();

    boost::asio::post([this, config = std::move(config)]() mutable {
        BOOST_LOG_NAMED_SCOPE("_receiveFromSocketLoop");

        TunnelFrameStream s(std::move(config.fd));
        InitialExchangeResult ier;

        try {
            ier = initialTunnelFrameExchange(s, _isClient);
            BOOST_LOG_TRIVIAL(info) << "Initial exchange with " << ier.identifier << " : "
                                    << ier.sessionId << " successful";
        } catch (const std::exception &ex) {
            BOOST_LOG_TRIVIAL(info)
                << "Initial exchange with " << s.toString() << " failed due to: " << ex.what();
            return;
        }

        std::unique_lock<std::mutex> ul(_mutex);

        auto &session = [&]() -> auto & {
            auto it = _sessions.find(ier.sessionId);
            if (it == _sessions.end())
                it = _sessions.emplace(ier.sessionId, ier.sessionId).first;
            return it->second;
        }
        ();

        auto it = session.streams.emplace(session.streams.end(), std::move(s));

        ScopedGuard sg([this, sessionId = ier.sessionId, itStream = it] {
            std::lock_guard<std::mutex> lg(_mutex);
            auto it = _sessions.find(sessionId);
            it->second.streams.erase(itStream);
            if (it->second.streams.empty())
                _sessions.erase(it);
        });

        auto &stream = *it;

        ul.unlock();

        try {
            _receiveFromSocketLoop(session, stream);

            RASSERT_MSG(false,
                        boost::format(
                            "Thread for socket %s exited normally. This should never be reached.") %
                            stream.toString());
        } catch (const std::exception &ex) {
            BOOST_LOG_TRIVIAL(info)
                << "Thread for socket " << stream.toString() << " completed due to " << ex.what();
        }
    });
}

void SocketProducerConsumer::onTunnelFrameFromPrev(TunnelFrameBuffer buf) {
    std::lock_guard<std::mutex> lg(_mutex);

    if (_sessions.empty())
        throw NotYetReadyException("The other side of the tunnel is not connected yet");

    // TODO: Choose a client/server connection to send it on based on some bandwidth requirements
    // metric, etc.
    _sessions.begin()->second.streams.front().send(buf);
}

void SocketProducerConsumer::onTunnelFrameFromNext(TunnelFrameBuffer buf) {
    RASSERT_MSG(false, "Socket producer consumer must be the last one in the chain");
}

void SocketProducerConsumer::_receiveFromSocketLoop(Session &session, TunnelFrameStream &stream) {
    while (true) {
        if (_interrupted.load()) {
            throw Exception("Interrupted");
        }

        auto buf = stream.receive();

        while (true) {
            if (_interrupted.load()) {
                throw Exception("Interrupted");
            }

            try {
                pipeInvokePrev(buf);
                break;
            } catch (const NotYetReadyException &ex) {
                BOOST_LOG_TRIVIAL(debug)
                    << "Tunnel not yet ready: " << ex.what() << "; retrying ...";
                ::sleep(5);
            }
        }
    }
}

SocketProducerConsumer::Session::Session(SessionId sessionId) : sessionId(std::move(sessionId)) {}

TunnelFrameStream::TunnelFrameStream(ScopedFileDescriptor fd) : _fd(std::move(fd)) {}

TunnelFrameStream::TunnelFrameStream(TunnelFrameStream &&) = default;

TunnelFrameStream::~TunnelFrameStream() = default;

void TunnelFrameStream::send(TunnelFrameBuffer buf) {
    int numWritten = 0;
    while (numWritten < buf.size) {
        numWritten += _fd.write((void const *)buf.data, buf.size);
    }

    BOOST_LOG_TRIVIAL(trace) << "Sent frame of " << numWritten << " bytes";
}

TunnelFrameBuffer TunnelFrameStream::receive() {
    size_t numRead = 0;

    while (numRead < sizeof(TunnelFrameHeaderInfo)) {
        numRead += _fd.read((void *)&_buffer[numRead], sizeof(TunnelFrameHeaderInfo) - numRead);
    }

    const auto &hdrInfo = TunnelFrameHeaderInfo::check({_buffer, numRead});
    const size_t totalSize = hdrInfo.desc.size;
    BOOST_LOG_TRIVIAL(trace) << "Received header of frame of size " << hdrInfo.desc.size
                             << " bytes";

    while (numRead < totalSize) {
        numRead += _fd.read((void *)&_buffer[numRead], totalSize - numRead);
    }

    RASSERT(numRead == totalSize);
    return {_buffer, totalSize};
}

} // namespace ruralpi
