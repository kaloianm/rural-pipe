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

from common import Service

if os.geteuid() != 0:
    exit("You need to have root privileges to run this script.\n"
         "Please try again, this time using 'sudo'. Exiting.")


class ClientService(Service):
    def __init__(self):
        super().__init__('client')

    def add_service_options(self, parser, config):
        pass

    async def pre_configure(self):
        pass

    async def post_configure(self):
        ipcmd = await asyncio.create_subprocess_shell(
            f'ip route add default via {str(self.ip_iface.ip)} dev rpic metric 0')
        await ipcmd.wait()


def main():
    os.chdir(sys.path[0])
    ClientService()


if __name__ == "__main__":
    main()
