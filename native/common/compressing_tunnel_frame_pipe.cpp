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

#include "common/base.h"

#include "common/compressing_tunnel_frame_pipe.h"

#include <boost/log/trivial.hpp>

namespace ruralpi {

CompressingTunnelFramePipe::CompressingTunnelFramePipe(TunnelFramePipe &prev)
    : TunnelFramePipe("Compressing") {
    pipePush(prev);
    BOOST_LOG_TRIVIAL(info) << "Compressing pipe attached";
}

CompressingTunnelFramePipe::~CompressingTunnelFramePipe() {
    pipePop();
    BOOST_LOG_TRIVIAL(info) << "Compressing pipe detached";
}

void CompressingTunnelFramePipe::onTunnelFrameFromPrev(TunnelFrameBuffer buf) {
    // TODO: Compress the contents of buf
    pipeInvokeNext(buf);
}

void CompressingTunnelFramePipe::onTunnelFrameFromNext(TunnelFrameBuffer buf) {
    // TODO: De-compress the contents of buf
    pipeInvokePrev(buf);
}

} // namespace ruralpi
