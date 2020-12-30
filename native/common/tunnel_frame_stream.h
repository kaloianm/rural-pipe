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

#include "common/scoped_file_descriptor.h"
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
     * Expects a closed tunnel frame writer, takes its contents and sends them on the socket. Will
     * block if the buffer of the socket is full.
     */
    void send(const TunnelFrameWriter &writer);

    /**
     * Receives a tunnel frame from the socket. Will block if there is no data available from the
     * socket yet.
     */
    TunnelFrameReader receive();

private:
    ScopedFileDescriptor _fd;

    uint8_t _buffer[kTunnelFrameMaxSize];
};

} // namespace ruralpi
