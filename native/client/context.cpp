
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

#include "client/context.h"

#include <boost/log/trivial.hpp>

namespace ruralpi {
namespace client {

namespace po = boost::program_options;

Context::Context() : ContextBase("client") {
    // clang-format off
    _desc.add_options()
        ("settings.server_host", po::value<std::string>()->required(), "Host on which the server is listening for connections")
        ("settings.server_port", po::value<int>()->default_value(50003), "Port on which the server is listening for connections")
        ("settings.interfaces", po::value<std::vector<std::string>>()->multitoken()->required(), "Set of interfaces through which to establish connections to the server")
    ;
    // clang-format on
}

Context::~Context() = default;

Context::ShouldStart Context::start(int argc, const char *argv[]) {
    ShouldStart shouldStart =
        ContextBase::start(argc, argv, [this](auto args) { return _onCommand(args); });
    if (shouldStart == kYes) {
        serverHost = _vm["settings.server_host"].as<std::string>();
        serverPort = _vm["settings.server_port"].as<int>();
        interfaces = _vm["settings.interfaces"].as<std::vector<std::string>>();
    }

    return shouldStart;
}

std::string Context::_onCommand(const std::vector<std::string> &args) { return "NOT IMPLEMENTED"; }

} // namespace client
} // namespace ruralpi
