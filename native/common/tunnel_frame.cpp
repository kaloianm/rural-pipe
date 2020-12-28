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

#include "common/tunnel_frame.h"

#include <cassert>
#include <cstring>

#include "common/exception.h"

namespace ruralpi {

using boost::format;

namespace {

const uint8_t kVersion = 1;

} // namespace

TunnelFrameReader::TunnelFrameReader(uint8_t const *data, size_t size)
    : _begin(data), _current(_begin), _end(_begin + size) {
    const auto &hdr = header();
    if (hdr.desc.version != kVersion)
        throw Exception(format("Unrecognised tunnel frame version %1%") % hdr.desc.version);
}

bool TunnelFrameReader::next() {
    if (_current == _begin)
        _current += sizeof(TunnelFrameHeader);
    else if (size() > 0)
        _current += sizeof(TunnelFrameDatagramSeparator) + size();

    return size() > 0;
}

TunnelFrameWriter::TunnelFrameWriter(uint8_t *data, size_t size)
    : _begin(data), _current(_begin + sizeof(TunnelFrameHeader)), _end(_begin + size) {}

void TunnelFrameWriter::onDatagramWritten(size_t size) {
    ((TunnelFrameDatagramSeparator *)_current)->size = size;
    _current += sizeof(TunnelFrameDatagramSeparator) + size;
}

void TunnelFrameWriter::close(uint64_t seqNum) {
    onDatagramWritten(0);

    auto &hdr = *((TunnelFrameHeader *)_begin);
    hdr.desc.version = kVersion;
    hdr.desc.flags = 0;
    hdr.desc.size = totalSize();
    // TODO: Properly populate the signature field
    strcpy((char *)&hdr.signature, "---- RURAL PIPE SIGNATURE ----");
    hdr.seqNum = seqNum;
}

void TunnelFrameWriter::appendBytes(uint8_t const *ptr, size_t size) {
    memcpy((void *)data(), (void *)ptr, size);
    onDatagramWritten(size);
}

void TunnelFrameWriter::appendString(const char str[]) {
    appendBytes((uint8_t const *)str, strlen(str) + 1);
}

TunnelFramePipe::TunnelFramePipe() = default;

void TunnelFramePipe::pipeTo(TunnelFramePipe &pipe) {
    assert(_pipe == nullptr);
    assert(pipe._pipe == nullptr);

    _pipe = &pipe;
    pipe._pipe = this;
}

void TunnelFramePipe::unPipe() {
    _pipe->_pipe = nullptr;
    _pipe = nullptr;
}

} // namespace ruralpi
