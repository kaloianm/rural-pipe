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

#include "common/exception.h"

namespace ruralpi {

namespace asio = boost::asio;
namespace fs = boost::filesystem;

namespace {

auto makeAcceptor(asio::io_service &ioService, const std::string &socketName) {
    std::string socketPath((fs::temp_directory_path() / socketName).string());
    BOOST_LOG_TRIVIAL(debug) << "Constructing command server acceptor: " << socketPath;

    try {
        SYSCALL(::unlink(socketPath.c_str()));
    } catch (const FileNotFoundSystemException &ex) {
        // Ignore if the socket path doesn't exist
    }

    auto acceptor = asio::local::stream_protocol::acceptor(
        ioService, asio::local::stream_protocol::endpoint(socketPath));

    return acceptor;
}

} // namespace

CommandsServer::CommandsServer(asio::io_service &ioService, std::string pipeName,
                               OnCommandFn onCommand)
    : _pipeName(std::move(pipeName)), _onCommand(std::move(onCommand)), _ioService(ioService),
      _acceptor(makeAcceptor(_ioService, _pipeName)) {
    _acceptNext();
}

CommandsServer::~CommandsServer() {}

void CommandsServer::_acceptNext() {
    auto conn = std::make_shared<UNIXConnection>(_ioService);

    _acceptor.async_accept(conn->socket(), [this, conn](const boost::system::error_code &error) {
        BOOST_LOG_TRIVIAL(debug) << "Accepted connection: " << error;

        if (!error) {
            conn->start();
        }

        _acceptNext();
    });
}

} // namespace ruralpi
