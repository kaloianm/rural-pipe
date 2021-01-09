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
#include <sys/stat.h>
#include <sys/types.h>

namespace ruralpi {

class FileDescriptor {
public:
    FileDescriptor(const std::string &desc, int fd);

    int read(void *buf, size_t nbytes);
    int write(void const *buf, size_t size);

    operator int() const { return _fd; }

    const auto &desc() const { return _desc; }

protected:
    FileDescriptor();

    // Description used for debugging and diagnostics purposes
    std::string _desc;
    int _fd{-1};
};

class ScopedFileDescriptor : public FileDescriptor {
public:
    ScopedFileDescriptor(const std::string &desc, int fd);

    ScopedFileDescriptor(ScopedFileDescriptor &&);
    ~ScopedFileDescriptor();
};

} // namespace ruralpi
