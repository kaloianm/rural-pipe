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
#include <boost/static_assert.hpp>
#include <boost/uuid/uuid.hpp>
#include <mutex>

namespace ruralpi {

// Maximum size of each tunnel frame. Actual frames might actually be smaller than that.
static constexpr size_t kTunnelFrameMaxSize = 4096;

struct ConstTunnelFrameBuffer {
    uint8_t const *data;
    size_t size;
};

struct TunnelFrameBuffer {
    uint8_t *data;
    size_t size;
};

#pragma pack(push, 1)

// Every tunnel frame header starts with this informational section
struct TunnelFrameHeaderInfo {
    static constexpr char kMagic[3] = {'R', 'P', 'I'};

    // Fix magic value to differentiate the start of each tunnel frame (always set to `kMagic`)
    char magic[3];

    struct Desc {
        uint16_t version : 2;
        uint16_t flags : 2;
        uint16_t size : 12;
    } desc;

    static const TunnelFrameHeaderInfo &check(const ConstTunnelFrameBuffer &buf);
};
BOOST_STATIC_ASSERT(sizeof(TunnelFrameHeaderInfo) == 5);

// Every tunnel frame starts with this header
struct TunnelFrameHeader : public TunnelFrameHeaderInfo {
    static constexpr uint64_t kInitialSeqNum = 0;

    // For which session does this frame apply
    boost::uuids::uuid sessionId;

    // Sequence number from the point of view of the sender of the frame. I.e., both client and
    // server send their own sequence numbers which need to be consecutive
    uint64_t seqNum;

    // Cryptographic signature of all the contents of the frame, which follow after this field (up
    // to `desc.size`)
    char signature[128];
};
BOOST_STATIC_ASSERT(sizeof(TunnelFrameHeader) == 157);

// The datagrams in each tunnel frame are separated with this structure
struct TunnelFrameDatagramSeparator {
    uint16_t size;
};
BOOST_STATIC_ASSERT(sizeof(TunnelFrameDatagramSeparator) == 2);

// The first tunnel frame exchanged between client and server (seqNum 0) contains a single
// "datagram" with this structure
struct InitTunnelFrame {
    char identifier[16];
};
BOOST_STATIC_ASSERT(sizeof(InitTunnelFrame) == 16);

#pragma pack(pop)

class TunnelFrameReader {
public:
    TunnelFrameReader(const ConstTunnelFrameBuffer &buf);
    TunnelFrameReader(const TunnelFrameBuffer &buf);

    /**
     * Provides direct access to the header of the frame and is available at any time, regardless of
     * whether the current pointer of the reader is located or whether there are any datagrams.
     */
    const TunnelFrameHeader &header() const { return *((TunnelFrameHeader *)_begin); }
    ConstTunnelFrameBuffer buffer() const { return {_begin, header().desc.size}; }

    /**
     * Must be called at least once to position the pointer at the first datagram in the frame. The
     * data() and size() below may only be accessed if it returns true.
     */
    bool next();

    uint8_t const *data() const { return _current + sizeof(TunnelFrameDatagramSeparator); }
    size_t size() const { return ((TunnelFrameDatagramSeparator *)_current)->size; }

private:
    uint8_t const *_begin;
    uint8_t const *_end;
    uint8_t const *_current;
};

class TunnelFrameWriter {
public:
    TunnelFrameWriter(const TunnelFrameBuffer &buf);

    /**
     * Provides direct access to the header of the frame and is available at any time, regardless of
     * whether the current pointer of the reader is located or whether there are any datagrams.
     */
    TunnelFrameHeader &header() const { return *((TunnelFrameHeader *)_begin); }
    TunnelFrameBuffer buffer() const { return {_begin, header().desc.size}; }

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
    void close();

    /**
     * These methods are here just to facilitate the writing of unit-tests and should not be used in
     * production code.
     */
    void appendBytes(uint8_t const *ptr, size_t size);
    void appendString(const char str[]);

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
     * The implementation is allowed to modify the passed-in buffer, but must not store any pointers
     * to any of its contents.
     *
     * The method must not block if the upstream called is unable to process the stream immediately.
     */
    virtual void onTunnelFrameReady(TunnelFrameBuffer buf) = 0;

    void pipeInvoke(TunnelFrameBuffer buf);

protected:
    TunnelFramePipe();
    TunnelFramePipe(TunnelFramePipe *pipe);

    void pipeAttach(TunnelFramePipe &pipe);
    void pipeDetach();

private:
    std::mutex _mutex;
    TunnelFramePipe *_pipe;
};

} // namespace ruralpi
