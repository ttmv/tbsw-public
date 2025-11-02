#!/bin/bash
#
stty -F /dev/ttyACM0 raw
stty -F /dev/ttyACM0 460800
#stty -F /dev/ttyACM0 > settingsublox.txt
echo "ubloxscript run"
stty -F /dev/ttyACM0
