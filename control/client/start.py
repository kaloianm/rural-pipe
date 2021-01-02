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

if os.geteuid() != 0:
    exit("You need to have root privileges to run this script.\n"
         "Please try again, this time using 'sudo'. Exiting.")


async def start_client_and_wait(options):
    print('Starting client service with options: ', options)

    if not os.path.exists('/tmp/client'):
        os.mkfifo('/tmp/client', mode=0o666)
    client_process = await asyncio.create_subprocess_exec(os.path.join(sys.path[0], 'client'),
                                                          stdin=asyncio.subprocess.PIPE,
                                                          stdout=asyncio.subprocess.PIPE,
                                                          stderr=asyncio.subprocess.STDOUT)
    first_line = await client_process.stdout.readline()
    if not first_line.decode().startswith('Rural Pipe client running'):
        print('Received unexpected response from the process:', first_line.decode())
        try:
            client_process.kill()
        except:
            pass
        print('Failed to start service')
        return await client_process.wait()

    print('Client started and active, configuring routing ...')

    ipcmd = await asyncio.create_subprocess_shell('ip link set rpic up')
    await ipcmd.wait()
    ipcmd = await asyncio.create_subprocess_shell('ip addr add ' + options.bind_ip + ' dev rpic')
    await ipcmd.wait()

    print('Routing configured')

    while True:
        line = await client_process.stdout.readline()
        if not line:
            break
        print(line.decode())

    return await client_process.wait()


def main():
    os.chdir(sys.path[0])

    config = ConfigParser()
    config_files_read = config.read('client.cfg')

    parser = OptionParser()
    parser.add_option('--bindip', dest='bind_ip', help='IP address to which to bind the network',
                      default=config.get('settings', 'bindip'))

    (options, args) = parser.parse_args()
    sys.exit(asyncio.run(start_client_and_wait(options)))


if __name__ == "__main__":
    main()
