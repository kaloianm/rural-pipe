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

#include "common/scoped_file_descriptor.h"

#include <boost/log/trivial.hpp>

#include "common/exception.h"

namespace ruralpi {

ScopedFileDescriptor::ScopedFileDescriptor(std::string desc, int fd)
    : _desc(std::move(desc)), _fd(fd) {
    if (_fd < 0) {
        Exception::throwFromErrno(boost::format("Could not open file descriptor (%d): %s") % _fd %
                                  _desc);
    }

    BOOST_LOG_TRIVIAL(debug) << boost::format("File descriptor created (%d): %s") % _fd % _desc;
}

ScopedFileDescriptor::~ScopedFileDescriptor() {
    if (_fd > 0) {
        BOOST_LOG_TRIVIAL(debug) << boost::format("File descriptor closed (%d): %s") % _fd % _desc;

        if (close(_fd) < 0) {
            BOOST_LOG_TRIVIAL(debug) << "Failed to close file descriptor " << _fd << " (" << _desc
                                     << "): " << Exception::getLastError();
        }

        return;
    } else if (_fd == -1) {
        return;
    }

    BOOST_LOG_TRIVIAL(fatal) << boost::format("Illegal file descriptor (%d): %s") % _fd %
                                    _desc.c_str();
    BOOST_ASSERT(false);
}

ScopedFileDescriptor::ScopedFileDescriptor(ScopedFileDescriptor &&other) {
    if (&other == this)
        return;

    BOOST_LOG_TRIVIAL(debug) << boost::format("File descriptor moved (%d): %s") % other._fd %
                                    other._desc;

    _desc = std::move(other._desc);
    _fd = other._fd;
    other._fd = -1;
}

} // namespace ruralpi
