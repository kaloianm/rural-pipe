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

#include <string>
#include <vector>

#include "common/file_descriptor.h"

namespace ruralpi {

/**
 * Creates the Tunnel device on construction (or throws) and closes all created file descriptors at
 * destruction time.
 */
class TunCtl {
    TunCtl(TunCtl &) = delete;

public:
    TunCtl(std::string deviceName, int numQueues);

    /**
     * Returns the MTU of the tunnel device.
     */
    size_t getMTU();

    /**
     * Returns the set of file descriptors mapped to this tunnel device's queues.
     */
    std::vector<FileDescriptor> getQueues() const;

private:
    std::string _deviceName;
    std::vector<FileDescriptor> _fds;
};

} // namespace ruralpi
