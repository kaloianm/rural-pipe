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

#include "client/tun_ctl.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "client/context.h"
#include "common/exception.h"

namespace ruralpi {
namespace {

const char kDeviceName[] = "RPI";

} // namespace

TunCtl::TunCtl(std::string deviceName, int numQueues)
    : _deviceName(std::move(deviceName)), _fileDescriptors(numQueues) {
    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    // Flags:   IFF_TUN   - TUN device (no Ethernet headers)
    //          IFF_NO_PI - Do not provide packet information
    //          IFF_MULTI_QUEUE - Create a queue of multiqueue device
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI | IFF_MULTI_QUEUE;
    strcpy(ifr.ifr_name, _deviceName.c_str());

    for (int i = 0; i < numQueues; i++) {
        if ((_fileDescriptors.fds[i] = open("/dev/net/tun", O_RDWR)) < 0)
            Exception::throwFromErrno("While creating tunnel device");
        if (ioctl(_fileDescriptors.fds[i], TUNSETIFF, (void *)&ifr))
            Exception::throwFromErrno("While configuring tunnel device");
    }
}

int TunCtl::operator[](int idx) const { return _fileDescriptors.fds[idx]; }

TunCtl::ScopedFileDescriptors::ScopedFileDescriptors(int numDescriptorsToAlloc)
    : fds(numDescriptorsToAlloc, -1) {}

TunCtl::ScopedFileDescriptors::~ScopedFileDescriptors() {
    for (int fd : fds) {
        close(fd);
    }
}

} // namespace ruralpi
