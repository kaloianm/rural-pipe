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

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <iostream>

#include "client/tun_ctl.h"

namespace po = boost::program_options;

namespace ruralpi {
namespace {

struct ClientInstanceEnvironment {
    ClientInstanceEnvironment(int argc, const char *argv[]) : desc("Client options") {
        desc.add_options()("help", "Produces this help message");

        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }

    bool help() const { return vm.count("help"); }

    po::options_description desc;
    po::variables_map vm;
};

int clientMain(const ClientInstanceEnvironment &env) {
    if (env.help()) {
        std::cout << env.desc;
        return 1;
    }

    std::cout << "Rural Pipe client" << std::endl;
    return 0;
}

} // namespace
} // namespace ruralpi

int main(int argc, const char *argv[]) {
    return ruralpi::clientMain(ruralpi::ClientInstanceEnvironment(argc, argv));
}
