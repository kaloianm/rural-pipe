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
#include <boost/uuid/uuid_io.hpp>

#include "common/exception.h"

namespace ruralpi {
namespace {

struct InitialExchangeResult {
    std::string identifier;
    SessionId sessionId;
};

InitialExchangeResult initialTunnelFrameExchange(TunnelFrameStream &stream,
                                                 const boost::optional<SessionId> clientSessionId) {
    uint8_t buffer[1024];
    memset(buffer, 0xAA, sizeof(buffer));

    if (clientSessionId) {
        TunnelFrameWriter writer({buffer, sizeof(buffer)});

        // The client generates the session, the server just accepts it
        writer.header().sessionId = *clientSessionId;
        writer.header().seqNum = TunnelFrameHeader::kInitFrameSeqNum;
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
        writer.header().seqNum = TunnelFrameHeader::kInitFrameSeqNum;
        strcpy(((char *)((InitTunnelFrame *)writer.data())->identifier), "RuralPipeServer");
        writer.onDatagramWritten(16);
        writer.close();
        stream.send(writer.buffer());

        return {std::string(((InitTunnelFrame const *)reader.data())->identifier),
                reader.header().sessionId};
    }
}

} // namespace

SocketProducerConsumer::SocketProducerConsumer(boost::optional<SessionId> clientSessionId,
                                               TunnelFramePipe &prev)
    : TunnelFramePipe("Socket"), _clientSessionId(std::move(clientSessionId)) {
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
    else {
        // This configuration allows up to the below configured number of frames to be placed in the
        // outgoing buffer for the socket before it will block. This ensures that the socket
        // selection algorithm will move on to the next available socket.
        {
            constexpr int kSendBufSize = 2 * kTunnelFrameMaxSize;
            SYSCALL(::setsockopt(config.fd, SOL_SOCKET, SO_SNDBUF, &kSendBufSize,
                                 sizeof(kSendBufSize)));

            constexpr int kTCPNoDelay = 1;
            SYSCALL(::setsockopt(config.fd, IPPROTO_TCP, TCP_NODELAY, &kTCPNoDelay,
                                 sizeof(kTCPNoDelay)));
        }

        int recvBufSize;
        {
            int recvBufSizeLen = sizeof(recvBufSize);
            SYSCALL(::getsockopt(config.fd, SOL_SOCKET, SO_RCVBUF, (void *)&recvBufSize,
                                 (socklen_t *)&recvBufSizeLen));
        }

        int sendBufSize;
        {
            int sendBufSizeLen = sizeof(sendBufSize);
            SYSCALL(::getsockopt(config.fd, SOL_SOCKET, SO_SNDBUF, (void *)&sendBufSize,
                                 (socklen_t *)&sendBufSizeLen));
        }

        BOOST_LOG_TRIVIAL(info) << config.fd.toString() << " socket buffer sizes: RCV "
                                << recvBufSize << " SND " << sendBufSize;
    }

    BOOST_LOG_TRIVIAL(info) << "Starting thread for socket file descriptor "
                            << config.fd.toString();

    boost::asio::post(_pool, [this, config = std::move(config)]() mutable {
        BOOST_LOG_NAMED_SCOPE("_receiveFromSocketLoop");

        TunnelFrameStream s(std::move(config.fd));
        InitialExchangeResult ier;

        try {
            ier = initialTunnelFrameExchange(s, _clientSessionId);
            BOOST_LOG_TRIVIAL(info) << "Initial exchange with " << ier.identifier << " : "
                                    << ier.sessionId << " successful";
        } catch (const std::exception &ex) {
            BOOST_LOG_TRIVIAL(info)
                << "Initial exchange with " << s.toString() << " failed due to: " << ex.what();
            return;
        }

        std::unique_lock ul(_mutex);

        // This reference is guaranteed to stay in place until 'sg' below is destroyed. This in turn
        // ensures that the only user of that reference (_receiveFromSocketLoop) will never access a
        // deleted pointer.
        auto &session = [&]() -> auto & {
            auto it = _sessions.find(ier.sessionId);
            if (it == _sessions.end()) {
                if (!_sessions.empty())
                    throw Exception("Currently only one session is supported per server instance");

                it = _sessions.emplace(ier.sessionId, ier.sessionId).first;
            }
            return it->second;
        }
        ();

        auto it = session.streams.emplace(session.streams.end(),
                                          std::make_shared<StreamTracker>(std::move(s)));
        auto st = *it;

        ScopedGuard sg([this, sessionId = ier.sessionId, itStream = it] {
            (*itStream)->stream.close();

            std::unique_lock ul(_mutex);

            auto it = _sessions.find(sessionId);
            it->second.streams.erase(itStream);

            bool eraseSession = it->second.streams.empty();
            if (eraseSession)
                _sessions.erase(it);

            ul.unlock();

            if (eraseSession)
                BOOST_LOG_TRIVIAL(info) << "Session " << sessionId << " closed";
        });

        ul.unlock();

        try {
            _receiveFromSocketLoop(session, st->stream);

            RASSERT_MSG(false,
                        boost::format(
                            "Thread for socket %s exited normally. This should never be reached.") %
                            st->stream.toString());
        } catch (const std::exception &ex) {
            BOOST_LOG_TRIVIAL(info) << "Thread for socket " << st->stream.toString()
                                    << " completed due to " << ex.what();
        }
    });
}

void SocketProducerConsumer::onTunnelFrameFromPrev(TunnelFrameBuffer buf) {
    std::shared_lock sl(_mutex);

    if (_sessions.empty())
        throw NotYetReadyException("The other side of the tunnel is not connected yet");

    auto &session = [&]() -> auto & {
        if (_clientSessionId) {
            RASSERT(_sessions.size() == 1);
            return _sessions.begin()->second;
        } else {
            // TODO: Keep mapping between source ports in order to differentiate the clients to
            // which packets need to be dispatched
            RASSERT(_sessions.size() == 1);
            return _sessions.begin()->second;
        }
    }
    ();

    TunnelFrameWriter::setSequenceNumberOnClosedBuffer(buf, session.nextSeqNum++);

    std::unique_lock ul(session.mutex);

    auto st = [&] {
        auto its = (Session::StreamsList::iterator *)alloca(session.streams.size());

        int i = 0;
        for (auto it = session.streams.begin(); it != session.streams.end(); it++, i++) {
            if (!(*it)->inUse) {
                return *it;
            }
            its[i] = it;
        }

        auto &st = *its[0];
        session.cv.wait(ul, [&] { return !st->inUse; });
        return st;
    }();

    st->inUse = true;
    st->bytesSending += buf.size;

    ul.unlock();
    sl.unlock();

    ScopedGuard sg([&] {
        sl.lock();
        ul.lock();

        st->inUse = false;
        st->bytesSending -= buf.size;
        session.cv.notify_one();
    });

    st->stream.send(buf);
    st->bytesSent += buf.size;
}

void SocketProducerConsumer::onTunnelFrameFromNext(TunnelFrameBuffer buf) {
    RASSERT_MSG(false, "Socket producer consumer must be the last one in the chain");
}

void SocketProducerConsumer::_receiveFromSocketLoop(Session &session, TunnelFrameStream &stream) {
    while (true) {
        if (_interrupted.load()) {
            throw Exception("Interrupted");
        }

        pipeInvokePrev(stream.receive());
    }
}

SocketProducerConsumer::Session::Session(SessionId sessionId) : sessionId(std::move(sessionId)) {}

TunnelFrameStream::TunnelFrameStream(ScopedFileDescriptor fd) : _fd(std::move(fd)) {
    _fd.makeNonBlocking();
}

TunnelFrameStream::TunnelFrameStream(TunnelFrameStream &&) = default;

TunnelFrameStream::~TunnelFrameStream() { close(); }

void TunnelFrameStream::close() { _fd.close(); }

void TunnelFrameStream::send(TunnelFrameBuffer buf) {
    int numWritten = 0;
    while (numWritten < buf.size) {
        numWritten += _fd.write((void const *)&buf.data[numWritten], buf.size - numWritten);
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
