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

#include "common/commands_server.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/log/trivial.hpp>

namespace ruralpi {

namespace fs = boost::filesystem;

namespace {} // namespace

CommandsServer::CommandsServer(boost::asio::io_service &ioService, std::string pipeName,
                               OnCommandFn onCommand)
    : _pipeName(std::move(pipeName)), _onCommand(std::move(onCommand)),
      _fstream((fs::temp_directory_path() / _pipeName)), _thread([this] { _commandLoop(); }) {
    boost::asio::local::stream_protocol::endpoint ep("/tmp/foobar");
}

CommandsServer::~CommandsServer() {}

void CommandsServer::_commandLoop() {
    std::string command;
    while (_fstream >> command) {
        BOOST_LOG_TRIVIAL(debug) << "Received command: " << command;

        [&]() noexcept {
            std::vector<std::string> tokens;
            _onCommand(boost::algorithm::split(tokens, command, boost::is_any_of("\t "),
                                               boost::token_compress_on));
        }();
    }
}

} // namespace ruralpi
