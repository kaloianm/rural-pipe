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

#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "common/commands_server.h"
#include "common/exception.h"

namespace ruralpi {

/**
 * There is a single instance of this object per executable and it represents the "context" of the
 * running process with things, such as the configuration options, exit code, the command server,
 * etc., hanging off of it.
 */
class ContextBase {
public:
    ContextBase(std::string serviceName);
    ~ContextBase();

    /**
     * Optional method to initialise the logging system and start the process. It can be skipped in
     * unit-tests in which case the logging will be to stdout/stderr.
     */
    enum ShouldStart { kYes, kHelpOnly };
    ShouldStart start(int argc, const char *argv[], CommandsServer::OnCommandFn onCommand);

    /**
     * Blocks until `exit` below is called.
     */
    int waitForExit();

    /**
     * Must be called in order to cause the process to terminate. Only the first invocation will be
     * taken into account, the following ones will be ignored
     */
    void exit(int exitCode);
    void exit(const Exception &ex);

    // Common configuration options
    int nqueues;

protected:
    boost::program_options::options_description _desc;
    boost::program_options::variables_map _vm;

private:
    const std::string _serviceName;

    // This is optional, so it can be constructed after the constructor of ContextBase has finished
    boost::optional<CommandsServer> _cmdServer;

    // Protects the mutable state below
    std::mutex _mutex;
    std::condition_variable _condVar;

    // Will be set when `exit` is called (on the first invocation of `exit` will be taken into
    // account, the following ones will be ignored)
    boost::optional<int> _exitCode;
};

} // namespace ruralpi
