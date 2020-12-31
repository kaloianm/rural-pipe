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

#include "common/tunnel_frame_stream.h"

#include <boost/log/trivial.hpp>

#include "common/exception.h"

namespace ruralpi {

TunnelFrameStream::TunnelFrameStream(ScopedFileDescriptor fd) : _fd(std::move(fd)) {}

TunnelFrameStream::~TunnelFrameStream() = default;

void TunnelFrameStream::send(const TunnelFrameWriter &writer) {
    int numWritten = 0;
    while (numWritten < writer.totalSize()) {
        numWritten += _fd.write((void *)writer.begin(), writer.totalSize());
    }

    BOOST_LOG_TRIVIAL(debug) << "Written frame of " << numWritten << " bytes";
}

TunnelFrameReader TunnelFrameStream::receive() {
    int numRead = _fd.read((void *)&_buffer[0], sizeof(TunnelFrameHeader));

    int totalSize = ((TunnelFrameHeader *)_buffer)->desc.size;
    BOOST_LOG_TRIVIAL(debug) << "Received a header of a frame of size " << totalSize;

    while (numRead < totalSize) {
        numRead += read(_fd, (void *)&_buffer[numRead], totalSize - numRead);

        BOOST_LOG_TRIVIAL(debug) << "Received " << numRead << " bytes so far";
    }

    BOOST_ASSERT(numRead == totalSize);
    return TunnelFrameReader(_buffer, totalSize);
}

} // namespace ruralpi
