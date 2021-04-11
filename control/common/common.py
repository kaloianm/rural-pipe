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
import ipaddress
import os
import sys

from configparser import ConfigParser
from optparse import OptionParser


# Controls the lifetime of a Service and allows steps from the instantiation to be overridden
class Service:
    def __init__(self, name):
        self.name = name
        self.fifo_path = os.path.join('/tmp', name)

        print(f'Starting service {self.name}')

        config = ConfigParser()
        config_files_read = config.read(f'{name}.cfg')
        if len(config_files_read) > 0:
            print(f'Reading options from {config_files_read}')

        # Add options which are common between the two services
        parser = OptionParser()
        parser.add_option('--tunnel_interface', dest='tunnel_interface',
                          help='The name to use for the tunnel interface device',
                          default=config.get('settings', 'tunnel_interface'))
        parser.add_option('--bind_ip', dest='bind_ip',
                          help='IP address to which to bind the network',
                          default=config.get('settings', 'bind_ip'))
        self.add_service_options(parser, config)

        (options, args) = parser.parse_args()
        self.options = options

        print(f'Service options: {options}')

        self.ip_iface = ipaddress.ip_interface(options.bind_ip)
        self.ip_network = ipaddress.ip_network(self.ip_iface.network)
        print('Network:', str(self.ip_network.network_address))

        # From this point onward, the service is starting
        if not os.path.exists(self.fifo_path):
            os.mkfifo(self.fifo_path, mode=0o666)

        # Asynchronous from here onwards
        asyncio.run(self.start_service_and_wait())

    # Starts the service executable asynchronously and returns a future, which will be signaled when
    # the service process has completed
    async def start_service_and_wait(self):
        print('Pre-configuring service')
        try:
            await self.pre_configure()
        except Exception as e:
            print(f'Service failed to start at pre-configuration due to {e}')
            raise

        self.service_process = await asyncio.create_subprocess_exec(
            os.path.join(sys.path[0], self.name), stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)

        try:
            first_line = await self.service_process.stdout.readline()
            if not first_line.decode().startswith('Rural Pipe running'):
                raise Exception('Received unexpected response from the process:',
                                first_line.decode())
        except Exception as e:
            print(f'Failed to start service due to {e}')
            self.service_process.kill()
            await self.service_process.wait()
            raise

        print('Service started and active, configuring ...')

        ipcmd = await asyncio.create_subprocess_shell(
            f'ip link set {self.options.tunnel_interface} up')
        await ipcmd.wait()

        ipcmd = await asyncio.create_subprocess_shell(
            f'ip addr add {str(self.ip_iface)} dev {self.options.tunnel_interface}')
        await ipcmd.wait()

        await self.post_configure()

        await asyncio.gather(self.service_process.wait())

    # Gives opportunity to the various service implementations to append their own custom options
    # to the parser arguments. This method must not throw.
    def add_service_options(self, parser, config):
        pass

    # Gives opportunity to service implementations to implement any pre-startup checks and
    # configuration before the service is started. This method is allowed to throw, but the
    # implementations are required to clean up anything that needs cleaning.
    async def pre_configure(self):
        pass

    async def post_configure(self):
        pass
