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
#include <list>
#include <mutex>
#include <string>
#include <thread>

#include "common/file_descriptor.h"
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

    const auto &desc() const { return _fd.desc(); }

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
    SocketProducerConsumer(bool isClient, TunnelFramePipe &pipe);
    ~SocketProducerConsumer();

    struct SocketConfig {
        ScopedFileDescriptor fd;
    };
    void addSocket(SocketConfig config);

private:
    // TunnelFramePipe methods
    void onTunnelFrameReady(TunnelFrameBuffer buf) override;

    /**
     * Runs on a thread per socket file descriptor passed through call to `addSocket`. Receives
     * incoming tunnel frames and passes them on to the upstream tunnel producer/consumer.
     */
    void _receiveFromSocketLoop(TunnelFrameStream &stream);

    // Indicates whether this socket is run as a client or server
    const bool _isClient;

    // Mutex to protect access to the state below
    std::mutex _mutex;

    // Associated with the `_receiveFromSocketLoop`
    std::atomic<bool> _interrupted{false};

    // Set of streams and associated threads to drain them
    std::list<TunnelFrameStream> _streams;
    std::list<std::thread> _threads;
};

} // namespace ruralpi
