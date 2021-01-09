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

using SessionId = boost::uuids::uuid;

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

    // Fixed magic value to differentiate the start of each tunnel frame (must always be populated
    // with the contents of `kMagic`)
    char magic[3];

    // Controls how the rest of the frame should be interpreted
    struct Desc {
        uint8_t version : 2;
        uint8_t flags : 6;
        uint16_t size;
    } desc;

    static const TunnelFrameHeaderInfo &check(const ConstTunnelFrameBuffer &buf);
};
BOOST_STATIC_ASSERT(sizeof(TunnelFrameHeaderInfo) == 6);

// Every tunnel frame starts with this header
struct TunnelFrameHeader : public TunnelFrameHeaderInfo {
    static constexpr uint64_t kInitialSeqNum = 0;

    // For which session does this frame apply
    SessionId sessionId;

    // Sequence number from the point of view of the sender of the frame. I.e., both client and
    // server send their own sequence numbers which need to be consecutive
    uint64_t seqNum;

    // Cryptographic signature of all the contents of the frame, which follow after this field (up
    // to `desc.size`)
    char signature[128];

    static const TunnelFrameHeader &cast(const ConstTunnelFrameBuffer &buf) {
        return *((TunnelFrameHeader const *)buf.data);
    }
    static TunnelFrameHeader &cast(const TunnelFrameBuffer &buf) {
        return *((TunnelFrameHeader *)buf.data);
    }
};
BOOST_STATIC_ASSERT(sizeof(TunnelFrameHeader) == 158);

// The datagrams in each tunnel frame are separated with this structure
struct TunnelFrameDatagramSeparator {
    uint16_t size;
};
BOOST_STATIC_ASSERT(sizeof(TunnelFrameDatagramSeparator) == 2);

constexpr size_t kTunnelFrameMinSize =
    sizeof(TunnelFrameHeader) + sizeof(TunnelFrameDatagramSeparator);
constexpr size_t kTunnelFrameMaxSize = 4096;

// The first tunnel frame exchanged between client and server (seqNum 0) contains a single
// "datagram" with this structure
struct InitTunnelFrame {
    // Contains a user-friendly descriptor of the side, which sends this frame
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
    const TunnelFrameHeader &header() const { return *((TunnelFrameHeader const *)_begin); }
    ConstTunnelFrameBuffer buffer() const { return {_begin, header().desc.size}; }

    /**
     * Must be called at least once to position the pointer at the first datagram in the frame. The
     * data() and size() below may only be accessed if it returns true.
     */
    bool next();

    /**
     * Returns the pointer where the current cursor (set by `next`) is pointing to and the size of
     * the datagram. This method is only safe to call if a previous call to `next()` has returned
     * true.
     */
    uint8_t const *data() const { return _current + sizeof(TunnelFrameDatagramSeparator); }
    size_t size() const { return ((TunnelFrameDatagramSeparator const *)_current)->size; }

private:
    uint8_t const *const _begin;
    uint8_t const *const _end;
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

    /**
     * Returns the biggest datagram which can be written to `data()`.
     */
    size_t remainingBytes() const;

    /**
     * Returns a pointer to a block of memory of maximum size `remainingBytes()` where the next
     * datagram should be written.
     */
    uint8_t *data() const { return _current + sizeof(TunnelFrameDatagramSeparator); }

    /**
     * Must be invoked every time datagram is written to `data()` and must be passed the size of the
     * datagram.
     */
    void onDatagramWritten(size_t size);

    /**
     * Must be invoked when no more datagrams will be written to that tunnel frame. It will update
     * the header of the frame and return the actual number of bytes which it contains.
     */
    void close();
    TunnelFrameBuffer buffer() const { return {_begin, header().desc.size}; }

    /**
     * These methods are here just to facilitate the writing of unit-tests and should be avoided in
     * production code since they perform copying.
     */
    void append(uint8_t const *ptr, size_t size);
    void append(const char str[]);
    void append(const std::string &str);

private:
    uint8_t *const _begin;
    uint8_t *const _end;
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

    /**
     * Invoke the `onTunnelFrameReady` method of the previous or next pipe in the chain.
     */
    void pipeInvokePrev(TunnelFrameBuffer buf);
    void pipeInvokeNext(TunnelFrameBuffer buf);

protected:
    TunnelFramePipe(std::string desc);
    TunnelFramePipe(std::string desc, TunnelFramePipe *prev, TunnelFramePipe *next);

    void pipePush(TunnelFramePipe &prev);
    void pipePop();

private:
    const std::string _desc;

    std::mutex _mutex;

    TunnelFramePipe *_prev;
    TunnelFramePipe *_next;
};

} // namespace ruralpi
