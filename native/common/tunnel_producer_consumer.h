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
#include <mutex>
#include <vector>

#include "common/file_descriptor.h"
#include "common/tunnel_frame.h"

namespace ruralpi {

/**
 * Handles the datagram communication with the tunnel device on the front and exchanges tunnel
 * frames through the pipe on the back.
 */
class TunnelProducerConsumer : public TunnelFramePipe {
public:
    TunnelProducerConsumer(std::vector<FileDescriptor> tunnelFds, int mtu);
    ~TunnelProducerConsumer();

private:
    // TunnelFramePipe methods
    void onTunnelFrameFromPrev(TunnelFrameBuffer buf) override;
    void onTunnelFrameFromNext(TunnelFrameBuffer buf) override;

    /**
     * One of these functions runs on a separate thread per tunnel file descriptor (from
     * `_tunnelFds`). They receive incoming datagrams, pack them into TunneFrame(s) and pass them
     * downwstream to the pipe attached to via 'TunnelFramePipe::pipeTo'.
     */
    void _receiveFromTunnelLoop(int idxTunnelFds);

    // Set of file descriptor provided a construction time, corresponding to the queues of the
    // tunnel device
    std::vector<FileDescriptor> _tunnelFds;
    int _mtu;

    // Used to select the output queue on which to send a datagram in a round-robin fashion
    std::atomic_uint64_t _tunnelFdRoundRobin{0};

    // Self-synchronising set of statistics for the tunnel interface
    struct Stats {
        Stats(int nTunnelFds) : bytesIn(nTunnelFds), bytesOut(nTunnelFds) {}

        // Global statistics

        // Per-tunnel queue statistics
        std::vector<std::atomic_uint64_t> bytesIn;
        std::vector<std::atomic_uint64_t> bytesOut;
    } _stats;

    // Set of threads draining the file descriptors from `_tunnelFds`
    boost::asio::thread_pool _pool;

    // Serves as a source for sequencing the tunnel frames
    std::atomic_uint64_t _seqNum{0};

    // Mutex to protect access to the state below
    std::mutex _mutex;

    // Associated with the `_receiveFromTunnelLoop` method
    std::atomic_bool _interrupted{false};
};

} // namespace ruralpi
