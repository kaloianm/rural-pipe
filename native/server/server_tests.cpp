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
#include <boost/log/trivial.hpp>
#include <mutex>

#include "common/exception.h"
#include "common/scoped_file_descriptor.h"
#include "common/socket_producer_consumer.h"
#include "common/tunnel_frame.h"
#include "common/tunnel_frame_stream.h"
#include "common/tunnel_producer_consumer.h"

namespace ruralpi {
namespace server {
namespace {

namespace fs = boost::filesystem;

#define LOG BOOST_LOG_TRIVIAL(info)
#define DATA_AND_SIZE(x) x, sizeof(x) + 1

struct TestFifo {
    ScopedFileDescriptor fd;

    TestFifo()
        : fd("Test FIFO", [] {
              auto fifoPath = fs::temp_directory_path() / "rural_pipe_test";
              remove(fifoPath.c_str());

              if (mkfifo(fifoPath.c_str(), 0666) < 0)
                  Exception::throwFromErrno("Unable to create the Fifo device");

              return open(fifoPath.c_str(), O_RDWR);
          }()) {}
};

void tunnelFrameTests() {
    uint8_t buffer[kTunnelFrameMaxSize];

    TunnelFrameWriter writer(buffer, kTunnelFrameMaxSize);
    LOG << writer.remainingBytes();
    writer.appendString("DG1");
    LOG << writer.remainingBytes();
    writer.appendString("DG2");
    LOG << writer.remainingBytes();
    writer.close(0);
    assert(writer.totalSize() > 0);
    LOG << writer.totalSize();

    TunnelFrameReader reader(buffer, kTunnelFrameMaxSize);
    assert(reader.header().seqNum == 0);
    assert(reader.next());
    LOG << (char *)reader.data();
    assert(reader.next());
    LOG << (char *)reader.data();
    assert(!reader.next());
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
            TunnelFrameWriter writer(buffer, sizeof(buffer));
            writer.appendString("DG1");
            writer.close(0);
            return writer;
        }());

        auto reader = stream.receive();
        assert(reader.header().seqNum == 0);
        assert(reader.next());
        LOG << (char *)reader.data();
        assert(!reader.next());
    }

    // Send/receive of a partial frame
    {
        TunnelFrameWriter writer(buffer, sizeof(buffer));
        for (int i = 0; i < 10; i++) {
            writer.appendString("Longer datagram content; Longer datagram content; Longer datagram "
                                "content; Longer datagram content; Longer datagram content; Longer "
                                "datagram content");
        }
        writer.close(0);
        LOG << "Size " << writer.totalSize();
        assert(write(fd, writer.begin(), 100) == 100);
        assert(write(fd, writer.begin() + 100, writer.totalSize() - 100) ==
               writer.totalSize() - 100);
        auto reader = stream.receive();
        assert(reader.header().seqNum == 0);
        for (int i = 0; i < 10; i++) {
            assert(reader.next());
            LOG << (char *)reader.data();
        }
        assert(!reader.next());
    }
}

void tunnelProducerConsumerTests() {
    TestFifo pipes[2];
    TunnelProducerConsumer tunnelPC(std::vector<int>{pipes[0].fd, pipes[1].fd});

    struct TestPipe : public TunnelFramePipe {
        TestPipe(TunnelFramePipe &pipe) { pipeTo(pipe); }
        ~TestPipe() { unPipe(); }

        void onTunnelFrameReady(TunnelFrameReader reader) override {
            LOG << "Received a frame of " << reader.totalSize() << " bytes";

            std::lock_guard<std::mutex> lg(mutex);
            ++numFramesReceived;
            memcpy(lastFrameReceived, reader.begin(), reader.totalSize());
            lastFrameReceivedSize = reader.totalSize();
        }

        void sendFrameBack(TunnelFrameReader reader) { _pipe->onTunnelFrameReady(reader); }

        std::mutex mutex;
        int numFramesReceived{0};
        uint8_t lastFrameReceived[kTunnelFrameMaxSize];
        size_t lastFrameReceivedSize{0};
    } testPipe(tunnelPC);

    LOG << "Sending datagrams on file descriptors " << pipes[0].fd << " " << pipes[1].fd;
    assert(write(pipes[0].fd, DATA_AND_SIZE("DG1.1")) > 0);
    assert(write(pipes[1].fd, DATA_AND_SIZE("DG2.1")) > 0);

    while (testPipe.numFramesReceived < 2)
        sleep(1);

    LOG << "Parsing the last received tunnel frame";

    TunnelFrameReader reader(testPipe.lastFrameReceived, testPipe.lastFrameReceivedSize);
    assert(reader.next());
    LOG << (char *)reader.data();
    assert(!reader.next());

    testPipe.sendFrameBack(
        TunnelFrameReader(testPipe.lastFrameReceived, testPipe.lastFrameReceivedSize));

    tunnelPC.interrupt();
}

void socketProducerConsumerTests() {
    TestFifo pipes[2];

    struct TestPipe : public TunnelFramePipe {
        void onTunnelFrameReady(TunnelFrameReader reader) override {}
    } testPipe;

    SocketProducerConsumer socketPC(true /* isClient */, testPipe);

    socketPC.interrupt();
}

void serverTestsMain() {
    BOOST_LOG_TRIVIAL(info) << "Running TunnelFrame tests ...";
    tunnelFrameTests();

    BOOST_LOG_TRIVIAL(info) << "Running TunnelFrameStream tests ...";
    tunnelFrameStreamTests();

    BOOST_LOG_TRIVIAL(info) << "Running TunnelProducerConsumer tests ...";
    tunnelProducerConsumerTests();

    BOOST_LOG_TRIVIAL(info) << "Running SocketProducerConsumer tests ...";
    socketProducerConsumerTests();
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
