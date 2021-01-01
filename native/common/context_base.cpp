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

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <iostream>

namespace ruralpi {

namespace fs = boost::filesystem;
namespace logging = boost::log;
namespace po = boost::program_options;

ContextBase::ContextBase(std::string serviceName)
    : _desc(boost::str(boost::format("RuralPipe %s options") % serviceName)),
      _serviceName(std::move(serviceName)) {
    // clang-format off
    _desc.add_options()
        ("help", "Produces this help message")
        ("settings.log", po::value<std::string>(), "The name of the log file to use. If missing, logging will be to the console.")
        ("settings.nqueues", po::value<int>()->default_value(1), "Number of queues/threads to instantiate to listen on the tunnel device")
    ;
    // clang-format on
}

ContextBase::~ContextBase() = default;

ContextBase::ShouldStart ContextBase::start(int argc, const char *argv[],
                                            CommandsServer::OnCommandFn onCommand) {
    // Parse the common options before initialising the logging system so that any errors are output
    // on the command line
    const fs::path configPath(fs::current_path() /
                              fs::path(boost::str(boost::format("%s.cfg") % _serviceName)));
    if (fs::exists(configPath))
        po::store(po::parse_config_file(configPath.c_str(), _desc, true /* allow_unregistered */),
                  _vm);
    po::store(po::parse_command_line(argc, argv, _desc), _vm);

    if (_vm.count("help")) {
        std::cout << _desc;
        exit(1);
        return kHelpOnly;
    }

    po::notify(_vm);

    nqueues = _vm["settings.nqueues"].as<int>();

    // Initialise the logging system
    if (_vm.count("settings.log")) {
        logging::add_file_log(logging::keywords::file_name =
                                  _vm["settings.log"].as<std::string>() + "_%N.log",
                              logging::keywords::rotation_size = 10 * 1024 * 1024,
                              logging::keywords::time_based_rotation =
                                  logging::sinks::file::rotation_at_time_point(0, 0, 0),
                              logging::keywords::auto_flush = true,
                              logging::keywords::format = "[%TimeStamp% (%Scope%)]: %Message%");
    } else {
        logging::add_console_log(std::cout, boost::log::keywords::format =
                                                "[%TimeStamp% (%Scope%)] %Message%");
    }

    logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);

    logging::add_common_attributes();
    logging::core::get()->add_global_attribute("Scope", boost::log::attributes::named_scope());

    // Instantiate the commands server so that the controlling script can start polling for startup
    // state information
    _cmdServer.emplace(_serviceName, std::move(onCommand));

    return kYes;
}

int ContextBase::waitForExit() {
    std::unique_lock<std::mutex> ul(_mutex);
    while (!_exitCode) {
        _condVar.wait(ul);
    }

    return *_exitCode;
}

void ContextBase::exit(int exitCode) {
    std::unique_lock<std::mutex> ul(_mutex);
    if (_exitCode)
        return;
    _exitCode = exitCode;
    _condVar.notify_all();
}

void ContextBase::exit(const Exception &ex) { exit(1); }

} // namespace ruralpi
