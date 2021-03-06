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

#include "common/tun_ctl.h"

#include <boost/log/trivial.hpp>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/ioctl.h>

#include "common/exception.h"

namespace ruralpi {
namespace {

const char kSystemTunnelDevice[] = "/dev/net/tun";

} // namespace

TunCtl::TunCtl(std::string deviceName, int numQueues) : _deviceName(std::move(deviceName)) {
    if (deviceName.length() >= IFNAMSIZ)
        throw Exception(boost::format("Device name %s is too long") % deviceName);

    struct ifreq ifr = {0};
    strcpy(ifr.ifr_name, _deviceName.c_str());

    // Flags:   IFF_TUN   - TUN device (no Ethernet headers)
    //          IFF_NO_PI - Do not provide packet information
    //          IFF_MULTI_QUEUE - Create a queue of multiqueue device
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI | IFF_MULTI_QUEUE;

    for (int i = 0; i < numQueues; i++) {
        _fds.emplace_back(kSystemTunnelDevice, ::open(kSystemTunnelDevice, O_RDWR));
        SYSCALL_MSG(::ioctl(_fds[i], TUNSETIFF, (void *)&ifr), "Error configuring tunnel device");
    }

    BOOST_LOG_TRIVIAL(info) << "Created tunnel device " << _deviceName << " with MTU " << getMTU();
}

size_t TunCtl::getMTU() {
    ScopedFileDescriptor sock("MTU check socket", ::socket(PF_INET, SOCK_DGRAM, IPPROTO_IP));
    struct ifreq ifr = {0};
    strcpy(ifr.ifr_name, _deviceName.c_str());
    SYSCALL(::ioctl(sock, SIOCGIFMTU, (void *)&ifr));
    return size_t(ifr.ifr_mtu);
}

std::vector<FileDescriptor> TunCtl::getQueues() const {
    std::vector<FileDescriptor> fds;
    for (auto &fd : _fds)
        fds.emplace_back(fd);
    return fds;
}

} // namespace ruralpi
