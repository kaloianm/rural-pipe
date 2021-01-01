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

#include <boost/asio/ip/address.hpp>
#include <linux/icmp.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <string>

namespace ruralpi {

template <typename T, typename S>
class StructCast : public S {
public:
    static const T &read(const void *p) { return *((const T *)p); }
    static const T &read(const char *p) { return read((const void *)p); }
};

struct ICMP : public StructCast<ICMP, icmphdr> {
    std::string toString() const;
};

struct TCP : public StructCast<TCP, tcphdr> {
    std::string toString() const;
};

struct UDP : public StructCast<UDP, udphdr> {
    std::string toString() const;
};

struct scopmcehdr {};

struct SSCOPMCE : public StructCast<SSCOPMCE, scopmcehdr> {
    static constexpr int kProtoNum = 128;

    std::string toString() const;
};

struct IP : public StructCast<IP, iphdr> {
    template <typename T>
    const T &as() const {
        return *((const T *)(((const char *)this) + sizeof(IP)));
    }

    std::string toString() const;
};

} // namespace ruralpi
