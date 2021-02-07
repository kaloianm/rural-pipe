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

#include <boost/filesystem.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <mutex>

#include "common/exception.h"
#include "common/socket_producer_consumer.h"
#include "common/tunnel_producer_consumer.h"
#include "test/test.h"

namespace ruralpi {
namespace test {
namespace {

namespace fs = boost::filesystem;

#define DATA_AND_SIZE(x) x, sizeof(x) + 1
#define CHECK(x) RASSERT(x)

struct TestFifo {
    ScopedFileDescriptor fd;

    TestFifo()
        : fd("Test FIFO", [] {
              auto fifoPath = fs::temp_directory_path() / "rural_pipe_test";
              ::remove(fifoPath.c_str());

              SYSCALL(::mkfifo(fifoPath.c_str(), 0666));
              return ::open(fifoPath.c_str(), O_RDWR);
          }()) {}
};

BOOST_AUTO_TEST_SUITE(TunnelFrameTests)

BOOST_AUTO_TEST_CASE(Tests) {
    uint8_t buffer[2 * kTunnelFrameMaxSize];
    memset(buffer, 0xAA, sizeof(buffer));

    {
        TLOG << "Small size datagram write";
        TunnelFrameWriter writer({buffer, kTunnelFrameMaxSize});
        TLOG << writer.remainingBytes();
        CHECK(writer.remainingBytes() == 3936); // Header + Separator
        writer.append("DG1");
        TLOG << writer.remainingBytes();
        writer.append("DG2");
        TLOG << writer.remainingBytes();
        writer.header().seqNum = 0;
        writer.close();
        CHECK(writer.buffer().size > 0);
        TLOG << writer.buffer().size << ": " << (int)writer.buffer().data[writer.buffer().size];
        CHECK(writer.buffer().data[writer.buffer().size] == 0xAA);
    }
    {
        TLOG << "Small size datagram read";
        TunnelFrameReader reader(ConstTunnelFrameBuffer{buffer, kTunnelFrameMaxSize});
        CHECK(reader.header().seqNum == 0);
        CHECK(reader.next());
        TLOG << reader.size() << ": " << (char *)reader.data();
        CHECK(reader.next());
        TLOG << reader.size() << ": " << (char *)reader.data();
        CHECK(!reader.next());
    }
    {
        TLOG << "Max size datagram write";
        TunnelFrameWriter writer({buffer, kTunnelFrameMaxSize});
        TLOG << writer.remainingBytes();
        writer.append(std::string(128, '-'));
        TLOG << writer.remainingBytes();
        writer.append(std::string(writer.remainingBytes(), '*'));
        TLOG << writer.remainingBytes();
        writer.header().seqNum = 1;
        writer.close();
        TLOG << writer.buffer().size;
        CHECK(writer.buffer().size == kTunnelFrameMaxSize);
        CHECK(writer.buffer().data[kTunnelFrameMaxSize] == 0xAA);
    }
    {
        TLOG << "Max size datagram read";
        TunnelFrameReader reader(ConstTunnelFrameBuffer{buffer, kTunnelFrameMaxSize});
        CHECK(reader.header().seqNum == 1);
        CHECK(reader.next());
        TLOG << reader.size();
        CHECK(reader.next());
        TLOG << reader.size();
        CHECK(!reader.next());
    }
    {
        TLOG << "Empty datagram write";
        TunnelFrameWriter writer({buffer, kTunnelFrameMaxSize});
        writer.header().seqNum = 2;
        writer.close();
        TLOG << writer.buffer().size;
    }
    {
        TLOG << "Empty datagram read";
        TunnelFrameReader reader(ConstTunnelFrameBuffer{buffer, kTunnelFrameMaxSize});
        CHECK(reader.header().seqNum == 2);
        CHECK(!reader.next());
    }
    {
        TLOG << "Random writes";
        for (int i = 0; i < 5000; i++) {
            TunnelFrameWriter writer({buffer, kTunnelFrameMaxSize});
            for (int numDatagramsToWrite = std::max(5, rand() % 1000);
                 numDatagramsToWrite == 0 || writer.remainingBytes() < 5; numDatagramsToWrite--) {
                writer.append(buffer,
                              std::min(size_t(5), size_t(rand() % writer.remainingBytes())));
            }
            writer.close();
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(TunnelFrameStreamTests)

BOOST_AUTO_TEST_CASE(Tests) {
    int fd;
    TunnelFrameStream stream([&] {
        TestFifo pipe;
        fd = pipe.fd;
        return std::move(pipe.fd);
    }());

    uint8_t buffer[kTunnelFrameMaxSize];
    memset(buffer, 0xAA, sizeof(buffer));

    // Send/receive
    {
        stream.send([&] {
            TunnelFrameWriter writer({buffer, sizeof(buffer)});
            writer.append("DG1");
            writer.header().seqNum = 0;
            writer.close();
            return writer.buffer();
        }());

        TunnelFrameReader reader(stream.receive());
        CHECK(reader.header().seqNum == 0);
        CHECK(reader.next());
        TLOG << (char *)reader.data();
        CHECK(!reader.next());
    }

    // Send/receive of a partial frame
    {
        TunnelFrameWriter writer({buffer, sizeof(buffer)});
        for (int i = 0; i < 10; i++) {
            writer.append(">>>> Longer datagram content; Longer datagram content; Longer datagram "
                          "content; Longer datagram content; Longer datagram content; Longer "
                          "datagram content; Longer datagram content; <<<<");
        }
        writer.header().seqNum = 0;
        writer.close();
        TLOG << "Size " << writer.buffer().size;
        CHECK(write(fd, writer.buffer().data, 100) == 100);
        CHECK(write(fd, writer.buffer().data + 100, writer.buffer().size - 100) ==
              writer.buffer().size - 100);

        TunnelFrameReader reader(stream.receive());
        CHECK(reader.header().seqNum == 0);
        for (int i = 0; i < 10; i++) {
            CHECK(reader.next());
            TLOG << (char *)reader.data();
        }
        CHECK(!reader.next());
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(TunnelProducerConsumerTests)

BOOST_AUTO_TEST_CASE(Tests) {
    TestFifo pipes[2];
    TunnelProducerConsumer tunnelPC(std::vector<FileDescriptor>{pipes[0].fd, pipes[1].fd}, 1500);

    struct TestPipe : public TunnelFramePipe {
        TestPipe(TunnelFramePipe &prev) : TunnelFramePipe("tunnelProducerConsumerTests") {
            pipePush(prev);
        }

        ~TestPipe() { pipePop(); }

        void onTunnelFrameReady(TunnelFrameBuffer buf) override {
            TLOG << "Received tunnel frame of " << buf.size << " bytes";

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

    TLOG << "Sending datagrams on file descriptors " << pipes[0].fd << " & " << pipes[1].fd;
    CHECK(pipes[0].fd.write(DATA_AND_SIZE("DG1.1")) > 0);
    CHECK(pipes[1].fd.write(DATA_AND_SIZE("DG2.1")) > 0);

    while (testPipe.getNumFramesReceived() < 2)
        ::sleep(1);

    TLOG << "Parsing the last received tunnel frame";

    TunnelFrameReader reader(
        ConstTunnelFrameBuffer{testPipe.lastFrameReceived, testPipe.lastFrameReceivedSize});
    CHECK(reader.next());
    TLOG << (char *)reader.data();
    CHECK(!reader.next());

    testPipe.pipeInvokePrev({testPipe.lastFrameReceived, testPipe.lastFrameReceivedSize});
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(SocketProducerConsumerTests)

BOOST_AUTO_TEST_CASE(Tests) {
    struct TestPipe : public TunnelFramePipe {
        TestPipe() : TunnelFramePipe("socketProducerConsumerTests") {}

        void onTunnelFrameReady(TunnelFrameBuffer buf) override {}
    } testPipe;

    SocketProducerConsumer socketPC(true /* isClient */, testPipe);

    TestFifo pipe;
    socketPC.addSocket(SocketProducerConsumer::SocketConfig{std::move(pipe.fd)});
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
} // namespace test
} // namespace ruralpi
