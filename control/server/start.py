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
import subprocess
import sys

from common import Service


class ServerService(Service):
    def __init__(self):
        super().__init__('server')

    def add_service_options(self, parser, config):
        parser.add_option('--wan_interface', dest='wan_interface',
                          help='Interface on which all traffic to the internet must go',
                          default=config.get('settings', 'wan_interface'))

    async def pre_configure(self):
        # Check if NAT is enabled for the server's WAN interface so that packets can flow
        ipcmd = await asyncio.create_subprocess_shell(
            f'iptables -C POSTROUTING -t nat -o {self.options.wan_interface} -j MASQUERADE')
        if await ipcmd.wait() > 0:
            raise Exception(f"""NAT is not configured. Please run the following command:
                                sudo iptables -A POSTROUTING -t nat -o {self.options.wan_interface} -j MASQUERADE"""
                            )

    async def post_configure(self):
        pass


def main():
    os.chdir(sys.path[0])
    ServerService()


if __name__ == "__main__":
    main()
