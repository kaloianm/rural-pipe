
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

#include "server/context.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace ruralpi {
namespace server {

namespace fs = boost::filesystem;
namespace po = boost::program_options;

Context::Context(Options options) : options(std::move(options)) {}

Options::Options(int argc, const char *argv[]) : _desc("Server options") {
    // clang-format off
    _desc.add_options()
        ("help", "Produces this help message")
        ("settings.port", po::value<int>()->default_value(50003), "Port on which to listen for connections")
    ;
    // clang-format on

    const fs::path configPath(fs::current_path() / fs::path("server.cfg"));
    po::store(po::parse_config_file(configPath.c_str(), _desc, true /* allow_unregistered */), _vm);
    po::store(po::parse_command_line(argc, argv, _desc), _vm);
    po::notify(_vm);

    port = _vm["settings.port"].as<int>();
}

bool Options::help() const { return _vm.count("help"); }

} // namespace server
} // namespace ruralpi
