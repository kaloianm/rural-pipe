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
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>

#include "common/exception.h"
#include "common/socket_producer_consumer.h"
#include "common/tunnel_frame.h"
#include "common/tunnel_producer_consumer.h"

namespace ruralpi {
namespace server {
namespace {

namespace fs = boost::filesystem;

#define LOG BOOST_LOG_TRIVIAL(info)
#define DATA_AND_SIZE(x) x, sizeof(x) + 1

void tunnelFrameTests() {
    uint8_t buffer[kTunnelFrameMaxSize];

    TunnelFrameWriter writer(buffer, kTunnelFrameMaxSize);
    LOG << writer.remainingBytes();
    strcpy((char *)writer.data(), "DG1");
    writer.onDatagramWritten(sizeof("DG1") + 1);
    LOG << writer.remainingBytes();
    strcpy((char *)writer.data(), "DG2");
    writer.onDatagramWritten(sizeof("DG2") + 1);
    LOG << writer.remainingBytes();
    writer.close(0);

    TunnelFrameReader reader(buffer, kTunnelFrameMaxSize);
    assert(reader.header().seqNum == 0);
    assert(reader.next());
    LOG << (char *)reader.data();
    assert(reader.next());
    LOG << (char *)reader.data();
    assert(!reader.next());
}

struct Fifo {
    Fifo() {
        auto fifoPath = fs::temp_directory_path() / "rural_pipe";
        remove(fifoPath.c_str());

        if (mkfifo(fifoPath.c_str(), 0666) < 0)
            Exception::throwFromErrno("Unable to create the Fifo device");

        fd = open(fifoPath.c_str(), O_RDWR);
        if (fd < 0)
            Exception::throwFromErrno("Unable to open the Fifo device");
    }

    int fd;
};

void tunnelProducerConsumerTests() {
    Fifo pipes[2];
    TunnelProducerConsumer tunnelPC(std::vector<int>{pipes[0].fd, pipes[1].fd});

    struct TestPipe : public TunnelFramePipe {
        void onTunnelFrameReady(void const *data, size_t size) override {
            LOG << "Received a frame of " << size << " bytes";

            std::lock_guard<std::mutex> lg(mutex);
            ++numFramesReceived;
            memcpy(lastFrameReceived, data, size);
            lastFrameReceivedSize = size;
        }

        void sendFrameBack(void const *data, size_t size) { _pipe->onTunnelFrameReady(data, size); }

        std::mutex mutex;
        int numFramesReceived{0};
        uint8_t lastFrameReceived[kTunnelFrameMaxSize];
        size_t lastFrameReceivedSize{0};
    } testPipe;

    tunnelPC.pipeTo(testPipe);
    tunnelPC.start();

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

    testPipe.sendFrameBack(testPipe.lastFrameReceived, testPipe.lastFrameReceivedSize);

    tunnelPC.stop();
    tunnelPC.unPipe();
}

void socketProducerConsumerTests() {}

void serverTestsMain() {
    BOOST_LOG_TRIVIAL(info) << "Running TunnelFrame tests ...";
    tunnelFrameTests();

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
