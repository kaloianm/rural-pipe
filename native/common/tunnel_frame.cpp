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

class NotYetReadyTunnelFramePipe : public TunnelFramePipe {
public:
    NotYetReadyTunnelFramePipe() : TunnelFramePipe("NotYetReady", nullptr, nullptr) {}

    void onTunnelFrameReady(TunnelFrameBuffer buf) {
        throw NotYetReadyException("Received frame before the pipe was configured");
    }

} kNotYetReadyTunnelFramePipe;

} // namespace

constexpr char TunnelFrameHeaderInfo::kMagic[3];

const TunnelFrameHeaderInfo &TunnelFrameHeaderInfo::check(const ConstTunnelFrameBuffer &buf) {
    if (buf.size < sizeof(TunnelFrameHeaderInfo))
        throw Exception(boost::format("Invalid tunnel frame header size %1%") % buf.size);

    const auto &hdrInfo = *((TunnelFrameHeaderInfo const *)buf.data);
    if (memcmp(hdrInfo.magic, TunnelFrameHeaderInfo::kMagic,
               sizeof(TunnelFrameHeaderInfo::kMagic) != 0))
        throw Exception(boost::format("Unrecognised tunnel frame magic number %1%") %
                        hdrInfo.magic);
    if (hdrInfo.desc.version != kVersion)
        throw Exception(boost::format("Unrecognised tunnel frame version %1%") %
                        hdrInfo.desc.version);
    if (hdrInfo.desc.size > kTunnelFrameMaxSize)
        throw Exception(boost::format("Invalid tunnel frame size %1%") % hdrInfo.desc.size);

    return hdrInfo;
}

TunnelFrameReader::TunnelFrameReader(const ConstTunnelFrameBuffer &buf)
    : _begin(buf.data), _current(_begin), _end(_begin + buf.size) {
    // TODO: Check the signature
}

TunnelFrameReader::TunnelFrameReader(const TunnelFrameBuffer &buf)
    : TunnelFrameReader(ConstTunnelFrameBuffer{buf.data, buf.size}) {}

bool TunnelFrameReader::next() {
    if (_current == _begin)
        _current += sizeof(TunnelFrameHeader);
    else if (size() > 0)
        _current += sizeof(TunnelFrameDatagramSeparator) + size();

    return size() > 0;
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
    int diff = _end - data();
    if (!diff)
        return 0;

    RASSERT_MSG(diff >= sizeof(TunnelFrameDatagramSeparator),
                boost::format("Invalid writer state with %d bytes left") % diff);
    return diff - sizeof(TunnelFrameDatagramSeparator);
}

void TunnelFrameWriter::onDatagramWritten(size_t size) {
    ((TunnelFrameDatagramSeparator *)_current)->size = size;
    _current += sizeof(TunnelFrameDatagramSeparator) + size;
    RASSERT(_current <= _end);
}

void TunnelFrameWriter::close() {
    onDatagramWritten(0);

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

TunnelFramePipe::TunnelFramePipe(std::string desc)
    : TunnelFramePipe(std::move(desc), &kNotYetReadyTunnelFramePipe, &kNotYetReadyTunnelFramePipe) {
}

TunnelFramePipe::TunnelFramePipe(std::string desc, TunnelFramePipe *prev, TunnelFramePipe *next)
    : _desc(std::move(desc)), _prev(prev), _next(next) {}

void TunnelFramePipe::pipeInvokePrev(TunnelFrameBuffer buf) {
    std::lock_guard<std::mutex> lg(_prev->_mutex);
    _prev->onTunnelFrameReady(buf);
}

void TunnelFramePipe::pipeInvokeNext(TunnelFrameBuffer buf) {
    std::lock_guard<std::mutex> lg(_next->_mutex);
    _next->onTunnelFrameReady(buf);
}

void TunnelFramePipe::pipePush(TunnelFramePipe &prev) {
    std::lock_guard<std::mutex> lgThis(_mutex);
    RASSERT(_prev == &kNotYetReadyTunnelFramePipe);
    RASSERT(_next == &kNotYetReadyTunnelFramePipe);

    std::lock_guard<std::mutex> lgPrev(prev._mutex);
    RASSERT(prev._next == &kNotYetReadyTunnelFramePipe);

    _prev = &prev;
    prev._next = this;
}

void TunnelFramePipe::pipePop() {
    std::lock_guard<std::mutex> lgThis(_mutex);
    RASSERT(_next == &kNotYetReadyTunnelFramePipe);

    std::lock_guard<std::mutex> lgPrev(_prev->_mutex);
    RASSERT(_prev->_next == this);

    _prev->_next = &kNotYetReadyTunnelFramePipe;
    _prev = &kNotYetReadyTunnelFramePipe;
}

} // namespace ruralpi
