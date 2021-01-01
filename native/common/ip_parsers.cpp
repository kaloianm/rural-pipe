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

#include "common/ip_parsers.h"

#include <sstream>

namespace ruralpi {

namespace asio = boost::asio;

std::string ICMP::toString() const {
    std::stringstream ss;
    ss << " type: " << uint8_t(type) << " code: " << uint8_t(code);
    return ss.str();
}

std::string TCP::toString() const {
    std::stringstream ss;
    ss << " sport: " << ntohs(source) << " dport: " << ntohs(dest) << " seq: " << ntohl(th_seq);
    return ss.str();
}

std::string UDP::toString() const {
    std::stringstream ss;
    ss << " sport: " << ntohs(source) << " dport: " << ntohs(dest) << " len: " << ntohs(len);
    return ss.str();
}

std::string SSCOPMCE::toString() const {
    std::stringstream ss;
    ss << "";
    return ss.str();
}

std::string IP::toString() const {
    std::stringstream ss;
    ss << " id: " << ntohs(id) << " proto: " << (int)protocol
       << " src: " << asio::ip::address_v4(ntohl(saddr))
       << " dst: " << asio::ip::address_v4(ntohl(daddr)) << " len: " << ntohs(tot_len);
    return ss.str();
}

} // namespace ruralpi
