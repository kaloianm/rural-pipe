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

#include "common/tunnel_frame.h"

#include <boost/log/trivial.hpp>
#include <cstring>

#include "common/exception.h"

namespace ruralpi {
namespace {

const uint8_t kVersion = 1;

} // namespace

constexpr char TunnelFrameHeaderInfo::kMagic[3];

const TunnelFrameHeaderInfo &TunnelFrameHeaderInfo::check(const ConstTunnelFrameBuffer &buf) {
    if (buf.size < sizeof(TunnelFrameHeaderInfo))
        throw Exception(boost::format("Invalid tunnel frame header size %1%") % buf.size);

    const auto &hdrInfo = *((TunnelFrameHeaderInfo const *)buf.data);
    if (memcmp(hdrInfo.magic, TunnelFrameHeaderInfo::kMagic,
               sizeof(TunnelFrameHeaderInfo::kMagic) != 0))
        throw Exception(boost::format("Unrecognised tunnel frame magic number %|.3s|") %
                        hdrInfo.magic);
    if (hdrInfo.desc.version != kVersion)
        throw Exception(boost::format("Unrecognised tunnel frame version %1%") %
                        hdrInfo.desc.version);
    if (hdrInfo.desc.size > kTunnelFrameMaxSize)
        throw Exception(boost::format("Invalid tunnel frame size %1%") % hdrInfo.desc.size);

    return hdrInfo;
}

TunnelFrameReader::TunnelFrameReader(const ConstTunnelFrameBuffer &buf) {
    const auto &hdrInfo = TunnelFrameHeaderInfo::check(buf);
    _begin = buf.data;
    _current = _begin;
    _end = _begin + hdrInfo.desc.size;
}

TunnelFrameReader::TunnelFrameReader(const TunnelFrameBuffer &buf)
    : TunnelFrameReader(ConstTunnelFrameBuffer{buf.data, buf.size}) {}

bool TunnelFrameReader::next() {
    if (_current == _begin)
        _current += sizeof(TunnelFrameHeader);
    else if (_current < _end)
        _current += sizeof(TunnelFrameDatagramSeparator) + size();

    if (_current > _end)
        throw Exception("Badly formatted frame");

    return _current < _end;
}

TunnelFrameWriter::TunnelFrameWriter(const TunnelFrameBuffer &buf)
    : _begin(buf.data), _current(_begin + sizeof(TunnelFrameHeader)), _end(_begin + buf.size) {
    RASSERT(buf.size >= kTunnelFrameMinSize);
    RASSERT(buf.size <= kTunnelFrameMaxSize);

    auto &hdr = header();
    memcpy(hdr.magic, TunnelFrameHeader::kMagic, sizeof(TunnelFrameHeader::kMagic));
    hdr.desc.version = kVersion;
    hdr.desc.flags = 0;
    hdr.desc.size = 0;
}

size_t TunnelFrameWriter::remainingBytes() const {
    if (_end - _current < sizeof(TunnelFrameDatagramSeparator))
        return 0;

    return _end - data();
}

void TunnelFrameWriter::onDatagramWritten(size_t size) {
    ((TunnelFrameDatagramSeparator *)_current)->size = size;
    _current += sizeof(TunnelFrameDatagramSeparator) + size;
    RASSERT_MSG(_current <= _end,
                boost::format("Writing a frame of size %1% resulted in invalid writer state: "
                              "_current = %2%, _end = %3%") %
                    size % (void *)_current % (void *)_end);
}

void TunnelFrameWriter::close() {
    auto &hdr = header();
    hdr.desc.size = _current - _begin;
}

void TunnelFrameWriter::append(uint8_t const *ptr, size_t size) {
    memcpy((void *)data(), (void *)ptr, size);
    onDatagramWritten(size);
}

void TunnelFrameWriter::append(const char str[]) { append((uint8_t const *)str, strlen(str) + 1); }

void TunnelFrameWriter::append(const std::string &str) {
    append((uint8_t const *)str.c_str(), str.size());
}

class TunnelFramePipe::NotYetReadyTunnelFramePipe : public TunnelFramePipe {
public:
    NotYetReadyTunnelFramePipe() : TunnelFramePipe("NotYetReady", nullptr, nullptr) {}

    void onTunnelFrameFromPrev(TunnelFrameBuffer buf) {
        throw NotYetReadyException("Received frame before the pipe was configured");
    }

    void onTunnelFrameFromNext(TunnelFrameBuffer buf) {
        throw NotYetReadyException("Received frame before the pipe was configured");
    }
};

TunnelFramePipe::TunnelFramePipe(std::string desc)
    : TunnelFramePipe(std::move(desc), &kNotYetReadyTunnelFramePipe, &kNotYetReadyTunnelFramePipe) {
}

TunnelFramePipe::NotYetReadyTunnelFramePipe TunnelFramePipe::kNotYetReadyTunnelFramePipe;

TunnelFramePipe::TunnelFramePipe(std::string desc, TunnelFramePipe *prev, TunnelFramePipe *next)
    : _desc(std::move(desc)), _prev(prev), _next(next) {}

void TunnelFramePipe::pipeInvokePrev(TunnelFrameBuffer buf) { _prev->onTunnelFrameFromNext(buf); }

void TunnelFramePipe::pipeInvokeNext(TunnelFrameBuffer buf) {
    std::unique_lock ul(_mutex);

    ++_numCallsToNext;
    TunnelFramePipe *next = _next;

    ul.unlock();

    ScopedGuard sg([&] {
        ul.lock();

        if (--_numCallsToNext == 0 && _nextIsDetaching)
            _cv.notify_all();
    });

    next->onTunnelFrameFromPrev(buf);
}

void TunnelFramePipe::pipePush(TunnelFramePipe &prev) {
    RASSERT(_prev == &kNotYetReadyTunnelFramePipe);
    RASSERT(_next == &kNotYetReadyTunnelFramePipe);
    RASSERT(prev._next == &kNotYetReadyTunnelFramePipe);

    _prev = &prev;
    prev._next = this;
}

void TunnelFramePipe::pipePop() {
    RASSERT(_next == &kNotYetReadyTunnelFramePipe);
    RASSERT(_prev->_next == this);

    {
        std::unique_lock ul(_prev->_mutex);
        _prev->_nextIsDetaching = true;
        _prev->_next = &kNotYetReadyTunnelFramePipe;
        _prev->_cv.wait(ul, [&] { return _prev->_numCallsToNext == 0; });
    }

    _prev = &kNotYetReadyTunnelFramePipe;
}

} // namespace ruralpi
