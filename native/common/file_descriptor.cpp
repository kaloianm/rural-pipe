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

#include "common/file_descriptor.h"

#include <boost/log/trivial.hpp>
#include <unistd.h>

#include "common/exception.h"

namespace ruralpi {

FileDescriptor::FileDescriptor(const std::string &desc, int fd) : _desc(desc), _fd(fd) {
    SYSCALL_MSG(_fd, boost::format("Could not open file descriptor (%d): %s") % _fd % _desc);
}

FileDescriptor::FileDescriptor() = default;

int FileDescriptor::read(void *buf, size_t nbytes) {
    int res =
        SYSCALL_MSG(::read(_fd, buf, nbytes),
                    boost::format("Failed to read from file descriptor (%d): %s") % _fd % _desc);
    if (res == 0)
        throw SystemException(boost::format("Failed to read from closed file descriptor (%d): %s") %
                              _fd % _desc);
    return res;
}

int FileDescriptor::write(void const *buf, size_t size) {
    return SYSCALL_MSG(::write(_fd, buf, size),
                       boost::format("Failed to write on file descriptor (%d): %s") % _fd % _desc);
}

ScopedFileDescriptor::ScopedFileDescriptor(const std::string &desc, int fd)
    : FileDescriptor(desc, fd) {
    BOOST_LOG_TRIVIAL(debug) << boost::format("File descriptor created (%d): %s") % _fd % _desc;
}

ScopedFileDescriptor::ScopedFileDescriptor(ScopedFileDescriptor &&other) {
    if (&other == this)
        return;

    _desc = std::move(other._desc);
    _fd = other._fd;
    other._fd = -1;
}

ScopedFileDescriptor::~ScopedFileDescriptor() {
    if (_fd > 0) {
        BOOST_LOG_TRIVIAL(debug) << boost::format("File descriptor closed (%d): %s") % _fd % _desc;

        if (close(_fd) < 0) {
            BOOST_LOG_TRIVIAL(debug)
                << (boost::format("File descriptor close failed (%d): %s") % _fd % _desc) << ": "
                << SystemException::getLastError();
        }

        return;
    } else if (_fd == -1) {
        return;
    }

    BOOST_LOG_TRIVIAL(fatal) << boost::format("Illegal file descriptor (%d): %s") % _fd %
                                    _desc.c_str();
    BOOST_ASSERT(false);
}

} // namespace ruralpi
