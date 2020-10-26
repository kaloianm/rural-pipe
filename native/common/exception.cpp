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

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <errno.h>
#include <string.h>

namespace ruralpi {

Exception::Exception(std::string message) : _message(std::move(message)) {}

const char *Exception::what() const noexcept { return _message.c_str(); }

void Exception::throwFromErrno(const std::string &context) {
    BOOST_ASSERT(errno);
    const int savedErrno = errno;

    constexpr size_t kErrTextSize = 4096;
    std::string errString(kErrTextSize, 0);
    if (context.empty())
        throw Exception(boost::str(boost::format("System error (%d): %s") % savedErrno %
                                   strerror_r(savedErrno, (char *)errString.data(), kErrTextSize)));
    else
        throw Exception(
            boost::str(boost::format("(%s): System error (%d): %s") % context % savedErrno %
                       strerror_r(savedErrno, (char *)errString.data(), kErrTextSize)));
}

void Exception::throwFromErrno() { throwFromErrno(""); }

} // namespace ruralpi
