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

#include "common/file_descriptor.h"

#include <boost/log/trivial.hpp>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common/exception.h"

namespace ruralpi {

FileDescriptor::FileDescriptor(const std::string &desc, int fd) : _desc(desc), _fd(fd) {
    SYSCALL_MSG(_fd, boost::format("Could not open file descriptor (%d): %s") % _fd % _desc);
}

FileDescriptor::FileDescriptor(const FileDescriptor &) = default;

FileDescriptor::FileDescriptor(FileDescriptor &&other) {
    _desc = std::move(other._desc);

    _fd = other._fd;
    other._fd = -1;
}

void FileDescriptor::makeNonBlocking() {
    int flags = SYSCALL(::fcntl(_fd, F_GETFL));
    SYSCALL(::fcntl(_fd, F_SETFL, flags | O_NONBLOCK));
}

int FileDescriptor::availableToRead() {
    int nAvailable;
    SYSCALL(::ioctl(_fd, FIONREAD, &nAvailable));
    return nAvailable;
}

int FileDescriptor::readNonBlocking(void *buf, size_t nbytes) {
    int nRead = ::read(_fd, buf, nbytes);
    if (nRead < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
        return nRead;

    SYSCALL_MSG(nRead, boost::format("Failed to read from file descriptor (%d): %s") % _fd % _desc);
    return nRead;
}

int FileDescriptor::read(void *buf, size_t nbytes) {
    while (true) {
        int nRead = readNonBlocking(buf, nbytes);
        if (nRead > 0) {
            return nRead;
        } else if (nRead == 0) {
            throw SystemException(
                boost::format("Failed to read from closed file descriptor (%d): %s") % _fd % _desc);
        } else if (nRead < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            poll(Milliseconds(-1), POLLIN);
        }
    }
}

int FileDescriptor::writeNonBlocking(void const *buf, size_t nbytes) {
    int nWritten = ::write(_fd, buf, nbytes);
    if (nWritten < 0 && (errno == EWOULDBLOCK || errno == EAGAIN))
        return nWritten;

    SYSCALL_MSG(nWritten,
                boost::format("Failed to write to file descriptor (%d): %s") % _fd % _desc);
    return nWritten;
}

int FileDescriptor::write(void const *buf, size_t nbytes) {
    while (true) {
        int nWritten = writeNonBlocking(buf, nbytes);
        if (nWritten > 0) {
            return nWritten;
        } else if (nWritten == 0) {
            throw SystemException(
                boost::format("Failed to write to closed file descriptor (%d): %s") % _fd % _desc);
        } else if (nWritten < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            poll(Milliseconds(-1), POLLOUT);
        }
    }
}

int FileDescriptor::poll(Milliseconds timeout, short events) {
    pollfd fd;
    fd.fd = _fd;
    fd.events = events;
    return SYSCALL(::poll(&fd, 1, timeout.count()));
}

ScopedFileDescriptor::ScopedFileDescriptor(const std::string &desc, int fd)
    : FileDescriptor(desc, fd) {
    BOOST_LOG_TRIVIAL(debug) << boost::format("File descriptor created (%d): %s") % _fd % _desc;
}

ScopedFileDescriptor::ScopedFileDescriptor(ScopedFileDescriptor &&other) = default;

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

    RASSERT_MSG(false, boost::format("Illegal file descriptor (%d): %s") % _fd % _desc.c_str());
}

} // namespace ruralpi
