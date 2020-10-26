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

target_choices = {
    'pc': {
        'arch': 'pc',
        'prefix': ''
    },
    'pi': {
        'arch': 'pi',
        'prefix': 'arm-linux-gnueabihf-'
    },
}

# --client_arch
AddOption('--client_arch', dest='client_arch', type='choice', choices=list(target_choices.keys()),
          default='pc', help='Use CLIENT_ARCH as the architecture for the client')

#
# END: command-line options for the build
###################################################################################################

env = Environment(
    ENV=os.environ,
    CLIENT_ARCH=target_choices[GetOption('client_arch')],
    SERVER_ARCH=target_choices['pc'],
)

SConscript('native/SConscript', exports='env', variant_dir='build/native', duplicate=0)
SConscript('control/SConscript', exports='env', variant_dir='build/control', duplicate=0)
