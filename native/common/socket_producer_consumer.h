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
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/tunnel_frame.h"

namespace ruralpi {

class SocketProducerConsumer : public TunnelFramePipe {
public:
    struct SocketConfig {
        // Human-readable name of the socket configuration (e.g., "eth0", "ADSL connection", "Mobile
        // connection")
        std::string name;

        // File descriptor of the socket
        int fd;
    };

    SocketProducerConsumer(bool isClient);
    ~SocketProducerConsumer();

    // These methods match the order of instantiation and destruction of the socket
    // producer/consumer in order to ensure there is no invalid memory accesses
    using TunnelFramePipe::pipeTo;
    void addSocket(SocketConfig config);
    void stop();
    using TunnelFramePipe::unPipe;

private:
    // TunnelFramePipe methods
    void onTunnelFrameReady(void const *data, size_t size) override;

    /**
     * Runs on a thread per socket file descriptor (from `_configs`). Receives incoming tunnel
     * frames and passes them on to the upstream tunnel producer/consumer.
     */
    void _receiveFromSocketLoop(int socketFd);

    // Indicates whether this socket is run as a client or server
    bool _isClient;

    // Mutex to protect access to the state below
    std::mutex _mutex;

    // Sockets for the outbound network connections
    std::vector<SocketConfig> _configs;

    // Associated with the `_receiveFromSocketLoop`
    std::atomic<bool> _receiveFromSocketTasksInterrupted{false};
    std::vector<std::thread> _receiveFromSocketThreads;
};

} // namespace ruralpi
