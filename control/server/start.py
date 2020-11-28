#!/usr/bin/env python3
#

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

import asyncio
import os
import sys

from configparser import ConfigParser
from optparse import OptionParser


async def start_server_and_wait(options):
    print('Starting server service with options: ', options)

    server_process = await asyncio.create_subprocess_exec(os.path.join(sys.path[0], 'server'),
                                                          stdin=asyncio.subprocess.PIPE,
                                                          stdout=asyncio.subprocess.PIPE,
                                                          stderr=asyncio.subprocess.STDOUT)
    first_line = await server_process.stdout.readline()
    if not first_line.decode().startswith('Rural Pipe server running'):
        print('Received unexpected response from the process:', first_line.decode())
        try:
            server_process.kill()
        except:
            pass
        print('Failed to start service')
        return await server_process.wait()

    print('Server started and active')

    while True:
        line = await server_process.stdout.readline()
        if not line:
            break
        print(line.decode())

    return await server_process.wait()


def main():
    os.chdir(sys.path[0])

    config = ConfigParser()
    config_files_read = config.read('server.cfg')

    parser = OptionParser()

    (options, args) = parser.parse_args()
    sys.exit(asyncio.run(start_server_and_wait(options)))


if __name__ == "__main__":
    main()