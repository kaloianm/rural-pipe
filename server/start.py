#!/usr/bin/env python3
#

import os

if os.geteuid() != 0:
    exit("You need to have root privileges to run this script.\n"
         "Please try again, this time using 'sudo'. Exiting.")

