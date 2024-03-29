#!/bin/bash

# Copyright 2020 Kaloian Manassiev
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
# associated documentation files (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge, publish, distribute,
# sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or
# substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
# NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <INSTALL DIRECTORY> <VERSION> <COMPILER>"
    exit 1
fi

export INSTALL_DIR="$1"
export VERSION="$2"
export COMPILER="$3"

if [ ! -d "$HOME/Downloads" ]; then
    echo "$0 requires $HOME/Downloads to be present, please make it"
    exit 1
fi

export BOOST_ARCHIVE="$HOME/Downloads/boost_${VERSION//./_}.tar.bz2"

if [ ! -f "$BOOST_ARCHIVE" ]; then
    echo "Downloading Boost archive to $BOOST_ARCHIVE ..."
    wget --directory-prefix "$HOME/Downloads" "https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_${VERSION//./_}.tar.bz2"
fi

echo "Extracting Boost archive $BOOST_ARCHIVE ..."
export EXTRACT_DIR=$(mktemp -d -t BoostXXX)
tar jxf $BOOST_ARCHIVE --directory $EXTRACT_DIR
export BOOST_ROOT=$EXTRACT_DIR/$(ls -U -1 $EXTRACT_DIR | head -1)

echo "Building Boost in $BOOST_ROOT ..."
pushd $BOOST_ROOT
./bootstrap.sh --with-libraries=log,program_options
echo "using gcc : platform : /usr/bin/${COMPILER} : <cxxflags>-std=c++14 ;" >./tools/build/src/user-config.jam
./b2 -d+2 --prefix=$INSTALL_DIR/boost --exec-prefix=$INSTALL_DIR/boost toolset=gcc-platform link=static install
./b2 -d+2 --with-test --prefix=$INSTALL_DIR/boost --exec-prefix=$INSTALL_DIR/boost toolset=gcc-platform link=static install
