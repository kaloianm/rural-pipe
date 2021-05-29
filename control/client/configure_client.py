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

import argparse
import fileinput
import os
import shlex
import subprocess

if os.geteuid() != 0:
    exit("You need to have root privileges to run this script.\n"
         "Please try again, this time using 'sudo'. Exiting.")

# List of interfaces that we will support
WAN = 'eth0'
USBWAN = 'usb0'
MOBILEWAN = 'wwan0'
LAN = ['eth1', 'eth2']
WLAN24GHZ = 'wlan0'
TUNNEL = 'rpic'

parser = argparse.ArgumentParser(description="""
This script performs all the necessary configuration steps for a clean-installed Raspberry Pi 4
device running Raspbian to be able to serve as a RuralPipe router. Recommended steps before running
it is to 'sudo apt update/upgrade' the installation to the latest packages.
""")
parser.add_argument('wifi_password', help='Password to set on the WiFi hotstpot')
options = parser.parse_args()


# Executes a single shell command and raises an exception containing the command itself and the
# code if it fails.
def shell_command(command):
    err_code = os.system(command)
    if err_code != 0:
        raise Exception(f'Command failed with code {err_code}: {command}')


# Rewrites the entire content of file_name with contents.
def rewrite_config_file(file_name, contents):
    file_name_for_backup = file_name + '.rural-pipe.orig'
    shell_command(f'[ ! -f {file_name} ] || cp {file_name} {file_name_for_backup}')

    with open(file_name, 'wt') as file:
        file.write(f"""
###################################################################################################
# THIS FILE {file_name} WAS AUTO GENERATED BY RuralPipe's configure_client.py SCRIPT
#
# DO NOT EDIT
###################################################################################################

{contents}""")


# Takes an option of the form 'option_name=option_value' and either updates it in the configuration
# file file_name or adds it if it doesn't exist.
def set_config_option(file_name, option):
    (option_name, option_value) = option.split('=')
    at_first_line = True
    option_set = False
    with fileinput.input(file_name, inplace=True) as file_name_input:
        for line in file_name_input:
            if at_first_line:
                print(
                    f"# THIS FILE {file_name} WAS MODIFIED BY RuralPupe's configure_client.py SCRIPT"
                )
                at_first_line = False
            if option_name in line:
                line = f'{option_name}={option_value}'
                option_set = True
            print(line, end='')
        if at_first_line:
            print(f"# THIS FILE {file_name} WAS MODIFIED BY RuralPupe's configure_client.py SCRIPT")
        if not option_set:
            with open(file_name, 'a') as file_name_input:
                file_name_input.write(f'{option_name}={option_value}')


# 0. Recommended first (manual) step after installing Raspbian is to run sudo apt update in order
# to obtain the latest versions of all packages.


# 1. Check the list of available network interfaces contains at least the WAN, MOBILEWAN, LAN and
# WLAN24GHZ interfaces that the rest of the script depends on.
#
# The USBWAN interface is hot-plug, so it won't be checked
def check_network_interfaces():
    p = subprocess.Popen(shlex.split('ifconfig -a -s'), stdout=subprocess.PIPE,
                         universal_newlines=True)
    lines = p.stdout.readlines()
    if not lines[0].startswith('Iface'):
        raise Exception('Unexpected output: ', lines[0])
    interfaces = list(map(lambda itf: itf.split()[0], lines[1:]))
    if not set([WAN, MOBILEWAN] + LAN + [WLAN24GHZ]).issubset(set(interfaces)):
        raise Exception('Unable to find required interfaces')


check_network_interfaces()

# 1. Install the required packages
shell_command(
    'echo iptables-persistent iptables-persistent/autosave_v4 boolean true | sudo debconf-set-selections'
)
shell_command(
    'echo iptables-persistent iptables-persistent/autosave_v6 boolean true | sudo debconf-set-selections'
)
shell_command(
    'apt install -y vim screen dnsmasq hostapd netfilter-persistent iptables-persistent bridge-utils openvpn libqmi-utils udhcpc'
)

# 2. Unblock the WLAN device(s) so they can transmit.
shell_command('rfkill unblock wlan')

# 3. Create the LAN bridge interface (WLAN will automatically be added to it when the HostAPD daemon
# starts up).
rewrite_config_file(
    '/etc/network/interfaces', f"""
auto lo
iface lo inet loopback

auto {WAN}
iface {WAN} inet dhcp
  metric 1

allow-hotplug {USBWAN}
iface {USBWAN} inet dhcp
  metric 2

manual {MOBILEWAN}
iface {MOBILEWAN} inet manual
  metric 3
  pre-up ifconfig {MOBILEWAN} down
  pre-up for _ in $(seq 1 10); do test -c /dev/cdc-wdm0 && break; /bin/sleep 1; done
  pre-up qmicli -d /dev/cdc-wdm0 --dms-set-operating-mode='online'
  pre-up for _ in $(seq 1 10); do qmicli -d /dev/cdc-wdm0 --nas-get-home-network && break; /bin/sleep 1; done
  pre-up qmi-network-raw /dev/cdc-wdm0 start
  pre-up udhcpc -i {MOBILEWAN}
  post-down qmi-network-raw /dev/cdc-wdm0 stop
  post-down qmicli -d /dev/cdc-wdm0 --dms-set-operating-mode='offline'
  post-down for _ in $(seq 1 10); do ! qmicli -d /dev/cdc-wdm0 --nas-get-home-network && break; /bin/sleep 1; done

{''.join(map(lambda ifname: f'iface {ifname} inet manual' + chr(10), LAN + [WLAN24GHZ]))}

auto br0
iface br0 inet static
  bridge_ports {''.join(map(lambda ifname: f'{ifname} ', LAN))}
  address 192.168.4.1
  broadcast 192.168.4.255
  netmask 255.255.255.0
  bridge_stp 0
  """)

# 4. Configure the DNS/DHCP server
rewrite_config_file(
    '/etc/dnsmasq.conf', f"""
interface=br0
dhcp-range=br0,192.168.4.50,192.168.4.200,255.255.255.0,12h

domain=rural

dhcp-authoritative

server=8.8.8.8
server=8.8.4.4
    """)

# 5. Configure and enable the hostapd daemon
rewrite_config_file(
    '/etc/hostapd/hostapd.conf', f"""
interface={WLAN24GHZ}
bridge=br0
hw_mode=a
ieee80211n=1
channel=36
ieee80211d=1
country_code=ES
wmm_enabled=1

ssid=RuralPipe (Kitchen)
# 1=wpa, 2=wep, 3=both
auth_algs=1
# WPA2 only
wpa=2
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
wpa_passphrase={options.wifi_password}""")

set_config_option('/etc/default/hostapd', 'DAEMON_CONF="/etc/hostapd/hostapd.conf"')

shell_command('sudo systemctl unmask hostapd')
shell_command('sudo systemctl enable hostapd')

# 6. Enable IP forwarding and NAT
set_config_option('/etc/sysctl.conf', 'net.ipv4.ip_forward=1')
shell_command(f'iptables -t nat -A POSTROUTING -o {WAN} -j MASQUERADE')
shell_command(f'iptables -t nat -A POSTROUTING -o {USBWAN} -j MASQUERADE')
shell_command(f'iptables -t nat -A POSTROUTING -o {MOBILEWAN} -j MASQUERADE')
shell_command(f'iptables -t nat -A POSTROUTING -o {TUNNEL} -j MASQUERADE')
shell_command('netfilter-persistent save')

# 7. Manual instructions for enabling USB Tethering with an Android Phone
#
# Plug phone into the USB port of the Android device and select "Internet Connection Sharing"
# instead of only charging, on the phone. This should show up the phone as a network
# interface called usb0 when `sudo ifconfig -a` is run.
#
# Bring the device up by running `sudo ifconfig usb0 up`.

# 8. Manual instructions for using OpenVPN with NordVPN
#
# TODO: Automate these instructions
#
# Follow the manual (OpenVPN) instructions at this link:
#   https://support.nordvpn.com/Connectivity/Linux/1047409422/How-can-I-connect-to-NordVPN-using-Linux-Terminal.htm
#
# Create a file with the NordVPN credentials as `$HOME/.nordvpn_cred`: First line is the user name,
# second line is the password.
#
# Connect using the following command line:
#   `sudo openvpn --config /etc/openvpn/ovpn_udp/es63.nordvpn.com.udp.ovpn --auth-user-pass ~/.nordvpn_cred`.
