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

#pragma once

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <functional>
#include <string>
#include <thread>

#include "common/connection.h"
#include "common/file_descriptor.h"

namespace ruralpi {

/**
 * Single server, which accepts simple text commands (ending in new line) and invokes the
 * `onCommand` handler with the tokenised result.
 */
class CommandsServer {
public:
    // First argument is the command, the rest are its arguments
    using OnCommandFn = std::function<std::string(std::vector<std::string>)>;

    CommandsServer(boost::asio::io_service &ioService, std::string pipeName, OnCommandFn onCommand);
    ~CommandsServer();

private:
    void _acceptNext();

    std::string _pipeName;
    OnCommandFn _onCommand;

    boost::asio::io_service &_ioService;

    boost::asio::local::stream_protocol::acceptor _acceptor;
};

} // namespace ruralpi
