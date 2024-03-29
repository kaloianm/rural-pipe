# Rural Pipe

A Raspberry Pi-based client/server system to unify the bandwidth of multiple available networks in order to provide a single high(er) bandwidth network access in rural locations.

## BUILD

These installation and build instructions assume a development machine (DEV), separate from the Raspberry Pi (RPI), running Ubuntu 20.04.

1. (On DEV): Install the build essentials package: `sudo apt install build-essential git`
1. (On DEV): Install the ARM cross-compiler and debugger: `sudo apt install gcc-8-arm-linux-gnueabihf g++-8-arm-linux-gnueabihf gdb-multiarch` (it is important to install version 8 of the cross-compiler if using Raspbian Buster, because it lacks the newer glibc library)
1. (On DEV): Install SCons: `python3 -m pip install SCons`
1. (On RPI - Optional): Optionally install the GDB server on the Raspberry Pi to be able to remotely debug from the development PC: `sudo apt install gdbserver`
1. Clone this repository: `git clone git@github.com:kaloianm/rural-pipe.git`
1. Build the Boost binaries for both ARM (Raspberry Pi) and PC architectures: `scons --client_arch=pi --server_arch=pc build/native/pi/boost build/native/pc/boost`
1. Run `scons --client_arch=pi --server_arch=pc`
 
## INSTALLATION

1. Follow these [instructions](https://www.raspberrypi.org/documentation/configuration/wireless/access-point-routed.md) in order to configure the Raspberry Pi as a Wifi router (so it manages its own internal network)
1. TODO: Instructions to start the client/server

## ACKNOWLEDGEMENTS

Implemented on Mac, using [Microsoft Visual Studio Code](https://code.visualstudio.com/) and [Ubuntu 20.04](https://releases.ubuntu.com/20.04/s) running under [Parallels Desktop for Mac](https://www.parallels.com/products/desktop/).

## LICENSE

Copyright 2020 Kaloian Manassiev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
