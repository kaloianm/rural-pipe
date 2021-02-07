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

#include "common/base.h"

#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE Main
#include "test/test.h"

#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

namespace ruralpi {
namespace test {
namespace {

namespace logging = boost::log;

class LoggingInitialisation {
public:
    LoggingInitialisation() {
        logging::add_console_log(std::cout,
                                 boost::log::keywords::format = "[%ThreadID% (%Scope%)] %Message%");

        logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);

        logging::add_common_attributes();
        logging::core::get()->add_global_attribute("Scope", boost::log::attributes::named_scope());
    }
};

BOOST_GLOBAL_FIXTURE(LoggingInitialisation);

} // namespace
} // namespace test
} // namespace ruralpi
