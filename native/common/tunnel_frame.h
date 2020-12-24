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

#include <array>

namespace ruralpi {

// Maximum size of each tunnel frame. Actual frames might actually be smaller than that.
static constexpr size_t kTunnelFrameMaxSize = 4096;

#pragma pack(push, 1)
// Every tunnel frame starts with this header
struct TunnelFrameHeader {
    struct Desc {
        uint8_t version : 2;
        uint8_t flags : 6;
    } desc;
    uint8_t signature[128];
    uint64_t seqNum;
};

// The datagrams in each tunnel frame are separated with this
struct TunnelFrameDatagramSeparator {
    uint16_t size;
};
#pragma pack(pop)

class TunnelFrameReader {
public:
    TunnelFrameReader(uint8_t const *data, size_t size);

    /**
     * Provides direct access to the header of the frame and is available at any time, regardless of
     * whether the current pointer of the reader is located or whether there are any datagrams.
     */
    const TunnelFrameHeader &header() const { return *((TunnelFrameHeader *)_begin); }

    bool next();

    uint8_t const *data() { return _current + sizeof(TunnelFrameDatagramSeparator); }
    uint8_t size() { return ((TunnelFrameDatagramSeparator *)_current)->size; }

private:
    uint8_t const *_begin;
    uint8_t const *_end;
    uint8_t const *_current;
};

class TunnelFrameWriter {
public:
    TunnelFrameWriter(uint8_t *data, size_t size);

    /**
     * Returns a pointer to a block of memory of maximum size `remainingBytes()` where the next
     * datagram should be written.
     */
    uint8_t *data() const { return _current + sizeof(TunnelFrameDatagramSeparator); }

    /**
     * Returns the biggest datagram which can be written to `current()`.
     */
    size_t remainingBytes() const { return _end - data(); }

    /**
     * Must be invoked every time datagram is written to `current()` and must be passed the size of
     * the datagram.
     */
    void onDatagramWritten(size_t size);

    /**
     * Must be invoked when no more datagrams will be written to that tunnel frame. It will update
     * the header of the frame and return the actual number of bytes which it contains.
     */
    size_t close(uint64_t seqNum);

private:
    uint8_t *_begin;
    uint8_t *_end;
    uint8_t *_current;
};

/**
 * Interface for exchanging tunnel frames between two parties (namely the tunnel producer/consumer
 * and the client/server socket).
 */
class TunnelFramePipe {
public:
    /**
     * Invoked when one side of the queue has a frame ready to pass upstream. May be called by
     * multiple threads at a time and it is up to the implementer to provide internal
     * synchronisation if it is required.
     *
     * The method is allowed to block if the upstream called is unable to process the frame
     * immediately.
     */
    virtual void onTunnelFrameReady(void const *data, size_t size) = 0;

    void pipeTo(TunnelFramePipe &pipe);
    void unPipe();

protected:
    TunnelFramePipe();

    TunnelFramePipe *_pipe{nullptr};
};

} // namespace ruralpi
