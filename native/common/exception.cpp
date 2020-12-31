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

#include "common/exception.h"

#include <errno.h>
#include <string.h>

namespace ruralpi {

Exception::Exception(std::string message) : _message(std::move(message)) {}

Exception::Exception(boost::format formatter) : Exception(formatter.str()) {}

const char *Exception::what() const noexcept { return _message.c_str(); }

void SystemException::throwFromErrno(const std::string &context) {
    if (context.empty())
        throw SystemException(boost::format("System error %s") % getLastError());
    else
        throw SystemException(boost::format("(%s): System error %s") % context % getLastError());
}

void SystemException::throwFromErrno(const boost::format &context) { throwFromErrno(context.str()); }

void SystemException::throwFromErrno() { throwFromErrno(""); }

std::string SystemException::getLastError() {
    BOOST_ASSERT(errno);
    const int savedErrno = errno;

    char buf[1024];
    return boost::str(boost::format("(%d): %s") % savedErrno %
                      strerror_r(savedErrno, buf, sizeof(buf)));
}

} // namespace ruralpi
