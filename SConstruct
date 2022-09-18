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

import os

###################################################################################################
# BEGIN: command-line options for the build
#

supported_architectures = {
    'pc': {
        'arch': 'pc',
        'prefix': '',
        'suffix': ''
    },
    'pi': {
        'arch': 'pi',
        'prefix': 'arm-linux-gnueabihf-',
        'suffix': '-8'
    },
}

AddOption('--client_arch', default='pc', dest='client_arch', type='choice',
          choices=list(supported_architectures.keys()),
          help='Use CLIENT_ARCH as the architecture for the client')
AddOption('--server_arch', default='pc', dest='server_arch', type='choice',
          choices=list(supported_architectures.keys()),
          help='Use SERVER_ARCH as the architecture for the server')
AddOption('--dbg', action='append_const', dest='cflags', const='-ggdb -Og')
AddOption('--opt', action='append_const', dest='cflags', const='-O3')

#
# END: command-line options for the build
###################################################################################################

env = Environment(
    ENV=os.environ,
    ARCHITECTURES=supported_architectures,
    CLIENT_ARCH=GetOption('client_arch'),
    SERVER_ARCH=GetOption('server_arch'),
)
env.MergeFlags(GetOption('cflags'))

SConscript('native/SConscript', exports='env', variant_dir='build/native', duplicate=0)
SConscript('control/SConscript', exports='env', variant_dir='build/control', duplicate=0)
