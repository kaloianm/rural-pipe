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

#include "common/exception.h"

#include <errno.h>
#include <string.h>

namespace ruralpi {
namespace {

std::string errnoMsg(int e) {
    RASSERT(e);
    char buf[1024];
    return boost::str(boost::format("(%d): %s") % e % strerror_r(e, buf, sizeof(buf)));
}

} // namespace

Exception::Exception(std::string message) : _message(std::move(message)) {}

Exception::Exception(boost::format formatter) : Exception(formatter.str()) {}

const char *Exception::what() const noexcept { return _message.c_str(); }

void SystemException::throwFromErrno(const std::string &context) {
    const int savedErrno = errno;
    auto msg = context.empty()
                   ? (boost::format("System error %s") % errnoMsg(savedErrno))
                   : (boost::format("(%s): System error %s") % context % errnoMsg(savedErrno));

    switch (savedErrno) {
    case ENOENT:
        throw FileNotFoundSystemException(std::move(msg));
    case ECONNREFUSED:
        throw ConnRefusedSystemException(std::move(msg));
    default:
        throw SystemException(std::move(msg));
    };
}

void SystemException::throwFromErrno(const boost::format &context) {
    throwFromErrno(context.str());
}

void SystemException::throwFromErrno() { throwFromErrno(""); }

std::string SystemException::getLastError() { return errnoMsg(errno); }

ScopedGuard::~ScopedGuard() {
    if (_fn)
        _fn();
}

ScopedGuard::ScopedGuard(ScopedGuard &&other) = default;

} // namespace ruralpi
