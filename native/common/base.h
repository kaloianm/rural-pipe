/**
 * Copyright 2021 Kaloian Manassiev
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

#include <boost/format.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <chrono>

namespace ruralpi {

using Milliseconds = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;

/**
 * Determines at compile time whether the processor is little or big endian
 */
constexpr bool isLittleEndianCPU() {
    constexpr char c[sizeof(int)] = {1};
    constexpr int i = {1};
    return i == c[0];
}

/**
 * Assertions, which terminate the process
 */
void assertFailedNoReturn(const char condition[], const char location[], const char context[]);
void assertFailedNoReturn(const char condition[], const char location[], const boost::format &fmt);

// Assertions, which are evaluated in both debug and release mode

#define RASSERT_MSG(x, msgOrFmt)                                                                   \
    {                                                                                              \
        if (!(x))                                                                                  \
            assertFailedNoReturn(BOOST_PP_STRINGIZE(x), __FILE__ ":" BOOST_PP_STRINGIZE(__LINE__), \
                                                                                        msgOrFmt); \
    }

#define RASSERT(x) RASSERT_MSG(x, "")

} // namespace ruralpi
