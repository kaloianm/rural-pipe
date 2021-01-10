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

#include <boost/log/trivial.hpp>
#include <execinfo.h>

#include "common/base.h"

namespace ruralpi {

void assertFailedNoReturn(const char condition[], const char location[], const char context[]) {
    BOOST_LOG_TRIVIAL(fatal) << "Assertion condition \"" << condition << "\" failed at " << location
                             << ": " << context;
    void *backtraceSymbols[1024];
    int numSyms = ::backtrace(backtraceSymbols, 1024);
    if (numSyms <= 0) {
        BOOST_LOG_TRIVIAL(fatal) << "Unable to extract backtrace";
    } else {
        char **btSyms = ::backtrace_symbols(backtraceSymbols, numSyms);
        BOOST_LOG_TRIVIAL(fatal) << "BACKTRACE:";
        for (int i = 0; i < numSyms; i++) {
            BOOST_LOG_TRIVIAL(fatal) << btSyms[i];
        }
        ::free(btSyms);
    }

    abort();
}

void assertFailedNoReturn(const char condition[], const char location[], const boost::format &fmt) {
    assertFailedNoReturn(condition, location, boost::str(fmt).c_str());
}

} // namespace ruralpi
