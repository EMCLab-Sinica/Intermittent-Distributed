import subprocess
import re

usb_list = subprocess.check_output(["my-mspdebug tilib --usb-list"], shell=True).decode()

devices = dict()
for line in usb_list.split("\n"):
    dev = re.match(r" *(\d+:\d+) *2047:0013  \[serial: (.*)]$", line)
    if dev:
        devices[dev.group(1)] = dev.group(2)

print([(key, devices[key]) for key in sorted(devices.keys())])

