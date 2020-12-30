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

#include "common/context_base.h"

#include <boost/format.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>

namespace ruralpi {
namespace {

namespace logging = boost::log;

void initLogging(const std::string &serviceName) {
    logging::add_file_log(logging::keywords::file_name = serviceName + "_%N.log",
                          logging::keywords::rotation_size = 10 * 1024 * 1024,
                          logging::keywords::time_based_rotation =
                              logging::sinks::file::rotation_at_time_point(0, 0, 0),
                          logging::keywords::auto_flush = true,
                          logging::keywords::format = "[%TimeStamp%]: %Message%");

    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);

    logging::add_common_attributes();
}

} // namespace

ContextBase::ContextBase(std::string serviceName, CommandsServer::OnCommandFn onCommand) {
    initLogging(serviceName);

    _cmdServer.emplace(serviceName, std::move(onCommand));
}

ContextBase::~ContextBase() {}

int ContextBase::waitForCompletion() {
    std::unique_lock<std::mutex> ul(_mutex);
    while (!_retVal) {
        _condVar.wait(ul);
    }

    return *_retVal;
}

} // namespace ruralpi
