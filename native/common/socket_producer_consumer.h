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

#pragma once

#include <atomic>
#include <boost/asio/thread_pool.hpp>
#include <boost/functional/hash.hpp>
#include <condition_variable>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "common/compressing_tunnel_frame_pipe.h"
#include "common/file_descriptor.h"
#include "common/signing_tunnel_frame_pipe.h"
#include "common/tunnel_frame.h"

namespace ruralpi {

/**
 * Blocking interface, which takes ownership of a socket file descriptor and sends and receives
 * tunnel frames from it.
 */
class TunnelFrameStream {
public:
    // An established file descriptor, whose lifetime is owned by the tunnel frame stream after
    // construction
    TunnelFrameStream(ScopedFileDescriptor fd);
    TunnelFrameStream(TunnelFrameStream &&);
    ~TunnelFrameStream();

    /**
     * Expects a buffer pointing to a closed tunnel frame writer. Takes its contents and sends them
     * on the socket. Will block if the buffer of the socket is full.
     */
    void send(TunnelFrameBuffer buf);

    /**
     * Receives a tunnel frame from the socket. Will block if there is no data available from the
     * socket yet.
     */
    TunnelFrameBuffer receive();

    /**
     * Closes the underlying file description.
     */
    void close();

    /**
     * Returns a string represenation of the stream, for debugging purposes.
     */
    std::string toString() const { return _fd.toString(); }

private:
    ScopedFileDescriptor _fd;

    uint8_t _buffer[kTunnelFrameMaxSize];
};

/**
 * Handles the communication between client and server on the front and passes tunnel frames through
 * the pipe on the back.
 */
class SocketProducerConsumer : public TunnelFramePipe {
public:
    /**
     * Constructs a new socket producer/consumer either as a client (where clientSessionId is set)
     * or as a server (where clientSessionId is not set). The difference is that when run as a
     * client, no mapping will be kept between outbound requests in order to differentiate for which
     * clients they need to be dispatched.
     */
    SocketProducerConsumer(boost::optional<SessionId> clientSessionId, TunnelFramePipe &prev);
    ~SocketProducerConsumer();

    struct SocketConfig {
        ScopedFileDescriptor fd;
    };
    void addSocket(SocketConfig config);

private:
    // TunnelFramePipe methods
    void onTunnelFrameFromPrev(TunnelFrameBuffer buf) override;
    void onTunnelFrameFromNext(TunnelFrameBuffer buf) override;

    /**
     * Tracks the state of a particular stream under a given session.
     */
    struct StreamTracker {
        StreamTracker(TunnelFrameStream stream) : stream(std::move(stream)) {}

        TunnelFrameStream stream;

        bool inUse{false};
        size_t bytesSending{0};

        // Statistics tracking
        std::atomic_uint64_t bytesSent{0};
    };

    /**
     * Encapsulates the entire runtime state of a session between a client and server.
     */
    struct Session {
        Session(SessionId sessionId);

        Session(const Session &) = delete;
        Session(Session &&) = delete;

        const SessionId sessionId;

        std::atomic_uint64_t nextSeqNum{TunnelFrameHeader::kInitFrameSeqNum + 1};

        std::mutex mutex;
        std::condition_variable cv;

        using StreamsList = std::list<std::shared_ptr<StreamTracker>>;
        StreamsList streams;
    };
    using SessionsMap = std::unordered_map<SessionId, Session, boost::hash<SessionId>>;

    /**
     * Runs on a thread per socket file descriptor passed through call to `addSocket`. Receives
     * incoming tunnel frames and passes them on to the upstream tunnel producer/consumer.
     */
    void _receiveFromSocketLoop(Session &session, TunnelFrameStream &stream);

    // Indicates whether this socket is run as a client or server
    const boost::optional<SessionId> _clientSessionId;

    // Passthrough pipes to sign and check signatures, compress and decompress the exchanged tunnel
    // frames
    boost::optional<CompressingTunnelFramePipe> _compresser;
    boost::optional<SigningTunnelFramePipe> _signer;

    // Set of threads draining the streams from `_streams`
    boost::asio::thread_pool _pool;

    // Associated with the `_receiveFromSocketLoop` method
    std::atomic_bool _interrupted{false};

    // Mutex to protect access to the state below
    std::shared_mutex _mutex;

    // Set of streams to the connected clients or server
    SessionsMap _sessions;
};

} // namespace ruralpi
