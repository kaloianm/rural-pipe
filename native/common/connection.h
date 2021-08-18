/**
 * Copyright 2021 Kaloian Manassiev
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

#include <boost/asio.hpp>

namespace ruralpi {

/**
 * Contains the base functionality for the two types of connections supported.
 */
class ConnectionBase {
public:
    void start();

protected:
    ConnectionBase(boost::asio::io_service &ioService);
    virtual ~ConnectionBase();

    boost::asio::io_service &_ioService;
};

template <typename TSocket>
class Connection : public ConnectionBase {
public:
    Connection(boost::asio::io_service &ioService)
        : ConnectionBase(ioService), _socket(_ioService) {}

    TSocket &socket() { return _socket; }

private:
    TSocket _socket;
};

using UNIXConnection = Connection<boost::asio::local::stream_protocol::socket>;
using TCPConnection = Connection<boost::asio::ip::tcp::socket>;

} // namespace ruralpi
