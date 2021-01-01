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

#include <boost/filesystem.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <mutex>

#include "common/exception.h"
#include "common/socket_producer_consumer.h"
#include "common/tunnel_producer_consumer.h"

namespace ruralpi {
namespace server {
namespace {

namespace logging = boost::log;
namespace fs = boost::filesystem;

#define LOG BOOST_LOG_TRIVIAL(info)
#define DATA_AND_SIZE(x) x, sizeof(x) + 1
#define CHECK(x) BOOST_ASSERT(x)

struct TestFifo {
    ScopedFileDescriptor fd;

    TestFifo()
        : fd("Test FIFO", [] {
              auto fifoPath = fs::temp_directory_path() / "rural_pipe_test";
              remove(fifoPath.c_str());

              SYSCALL(mkfifo(fifoPath.c_str(), 0666));
              return open(fifoPath.c_str(), O_RDWR);
          }()) {}
};

void tunnelFrameTests() {
    uint8_t buffer[kTunnelFrameMaxSize];
    memset(buffer, 0xAA, sizeof(buffer));

    TunnelFrameWriter writer({buffer, kTunnelFrameMaxSize});
    LOG << writer.remainingBytes();
    writer.appendString("DG1");
    LOG << writer.remainingBytes();
    writer.appendString("DG2");
    LOG << writer.remainingBytes();
    writer.header().seqNum = 0;
    writer.close();
    CHECK(writer.buffer().size > 0);
    LOG << writer.buffer().size;
    CHECK(writer.buffer().data[writer.buffer().size] == 0xAA);

    TunnelFrameReader reader(ConstTunnelFrameBuffer{buffer, kTunnelFrameMaxSize});
    CHECK(reader.header().seqNum == 0);
    CHECK(reader.next());
    LOG << (char *)reader.data();
    CHECK(reader.next());
    LOG << (char *)reader.data();
    CHECK(!reader.next());
}

void tunnelFrameStreamTests() {
    int fd;
    TunnelFrameStream stream([&] {
        TestFifo pipe;
        fd = pipe.fd;
        return std::move(pipe.fd);
    }());

    uint8_t buffer[kTunnelFrameMaxSize];

    // Send/receive
    {
        stream.send([&] {
            TunnelFrameWriter writer({buffer, sizeof(buffer)});
            writer.appendString("DG1");
            writer.close();
            return writer.buffer();
        }());

        TunnelFrameReader reader(stream.receive());
        CHECK(reader.header().seqNum == 0);
        CHECK(reader.next());
        LOG << (char *)reader.data();
        CHECK(!reader.next());
    }

    // Send/receive of a partial frame
    {
        TunnelFrameWriter writer({buffer, sizeof(buffer)});
        for (int i = 0; i < 10; i++) {
            writer.appendString("Longer datagram content; Longer datagram content; Longer datagram "
                                "content; Longer datagram content; Longer datagram content; Longer "
                                "datagram content");
        }
        writer.close();
        LOG << "Size " << writer.buffer().size;
        CHECK(write(fd, writer.buffer().data, 100) == 100);
        CHECK(write(fd, writer.buffer().data + 100, writer.buffer().size - 100) ==
              writer.buffer().size - 100);
        TunnelFrameReader reader(stream.receive());
        CHECK(reader.header().seqNum == 0);
        for (int i = 0; i < 10; i++) {
            CHECK(reader.next());
            LOG << (char *)reader.data();
        }
        CHECK(!reader.next());
    }
}

void tunnelProducerConsumerTests() {
    TestFifo pipes[2];
    TunnelProducerConsumer tunnelPC(std::vector<FileDescriptor>{pipes[0].fd, pipes[1].fd}, 1500);

    struct TestPipe : public TunnelFramePipe {
        TestPipe(TunnelFramePipe &pipe) { pipeAttach(pipe); }
        ~TestPipe() { pipeDetach(); }

        void onTunnelFrameReady(TunnelFrameBuffer buf) override {
            LOG << "Received tunnel frame of " << buf.size << " bytes";

            std::lock_guard<std::mutex> lg(mutex);
            memcpy(lastFrameReceived, buf.data, buf.size);
            lastFrameReceivedSize = buf.size;
            ++numFramesReceived;
        }

        int getNumFramesReceived() {
            std::lock_guard<std::mutex> lg(mutex);
            return numFramesReceived;
        }

        std::mutex mutex;
        int numFramesReceived{0};
        uint8_t lastFrameReceived[kTunnelFrameMaxSize];
        size_t lastFrameReceivedSize{0};
    } testPipe(tunnelPC);

    LOG << "Sending datagrams on file descriptors " << pipes[0].fd << " " << pipes[1].fd;
    CHECK(write(pipes[0].fd, DATA_AND_SIZE("DG1.1")) > 0);
    CHECK(write(pipes[1].fd, DATA_AND_SIZE("DG2.1")) > 0);

    while (testPipe.getNumFramesReceived() < 2)
        sleep(1);

    LOG << "Parsing the last received tunnel frame";

    TunnelFrameReader reader(
        ConstTunnelFrameBuffer{testPipe.lastFrameReceived, testPipe.lastFrameReceivedSize});
    CHECK(reader.next());
    LOG << (char *)reader.data();
    CHECK(!reader.next());

    testPipe.pipeInvoke({testPipe.lastFrameReceived, testPipe.lastFrameReceivedSize});
}

void socketProducerConsumerTests() {
    TestFifo pipes[2];

    struct TestPipe : public TunnelFramePipe {
        void onTunnelFrameReady(TunnelFrameBuffer buf) override {}
    } testPipe;

    SocketProducerConsumer socketPC(true /* isClient */, testPipe);
}

void serverTestsMain() {
    logging::add_console_log(std::cout,
                             boost::log::keywords::format = "[%ThreadID% (%Scope%)] %Message%");
    logging::add_common_attributes();
    logging::core::get()->add_global_attribute("Scope", boost::log::attributes::named_scope());

    {
        BOOST_LOG_NAMED_SCOPE("tunnelFrameTests");
        BOOST_LOG_TRIVIAL(info) << "Running tunnelFrameTests ...";
        tunnelFrameTests();
    }

    {
        BOOST_LOG_NAMED_SCOPE("tunnelFrameTests");
        BOOST_LOG_TRIVIAL(info) << "Running tunnelFrameTests ...";
        tunnelFrameStreamTests();
    }

    {
        BOOST_LOG_NAMED_SCOPE("tunnelProducerConsumerTests");
        BOOST_LOG_TRIVIAL(info) << "Running tunnelProducerConsumerTests ...";
        tunnelProducerConsumerTests();
    }

    {
        BOOST_LOG_NAMED_SCOPE("socketProducerConsumerTests");
        BOOST_LOG_TRIVIAL(info) << "Running socketProducerConsumerTests ...";
        socketProducerConsumerTests();
    }
}

} // namespace
} // namespace server
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    try {
        ruralpi::server::serverTestsMain();
    } catch (const std::exception &ex) {
        BOOST_LOG_TRIVIAL(fatal) << "Error occurred: " << ex.what();
        return 1;
    }
}
