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

#include <fcntl.h>
#include <string>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>

namespace ruralpi {

/**
 * Wrapper around a file descriptor, which exposes the generic system calls as a set of
 * SystemException-throwing methods. The class doesn't own the file descriptor (meaning that it
 * won't close it at destruction time for example).
 *
 * This object is both movable and copiable, but care must be take with multiple copies, because the
 * underlying file descriptor can be closed at any time by any of the other owners.
 */
class FileDescriptor {
public:
    FileDescriptor(const std::string &desc, int fd);

    FileDescriptor(const FileDescriptor &);
    FileDescriptor(FileDescriptor &&);

    int availableToRead();
    int read(void *buf, size_t nbytes);
    int write(void const *buf, size_t size);

    int poll(std::chrono::milliseconds timeout);

    operator int() const { return _fd; }

    const std::string &toString() const { return _desc; }

protected:
    // Description used for debugging and diagnostics purposes
    std::string _desc;

    // The file descriptor that is being wrapped
    int _fd{-1};
};

/**
 * Extension of `FileDescriptor` above, which actually owns the passed-in file descriptor and closes
 * it at destruction time. This object is only movable, but not copiable.
 */
class ScopedFileDescriptor : public FileDescriptor {
public:
    ScopedFileDescriptor(const std::string &desc, int fd);

    ScopedFileDescriptor(ScopedFileDescriptor &&);
    ~ScopedFileDescriptor();
};

} // namespace ruralpi
