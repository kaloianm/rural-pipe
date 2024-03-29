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
import netifaces
import os
import sys

from common import Service

if os.geteuid() != 0:
    exit("You need to have root privileges to run this script.\n"
         "Please try again, this time using 'sudo'. Exiting.")


class ServerService(Service):
    def __init__(self):
        super().__init__('server')

    def add_service_options(self, parser, config):
        pass

    async def pre_configure(self):
        gws = netifaces.gateways()
        default_gateway = gws['default'][netifaces.AF_INET]

        # Check if NAT is enabled for the server's WAN interface so that packets can flow
        ipcmd = await asyncio.create_subprocess_shell(
            f'iptables -C POSTROUTING -t nat -o {default_gateway[1]} -j MASQUERADE')
        if await ipcmd.wait() > 0:
            raise Exception(
                f"NAT is not configured on the default gateway. Please run the following command: "
                f"sudo iptables -A POSTROUTING -t nat -o {default_gateway[1]} -j MASQUERADE")

    async def post_configure(self):
        pass


def main():
    os.chdir(sys.path[0])
    ServerService()


if __name__ == "__main__":
    main()
