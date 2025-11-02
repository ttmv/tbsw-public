#!/bin/bash


echo "running portfinder for ref imu"
portfile=refport.txt
if [ -f "$portfile" ]; then
  echo "removing old port info file"
  rm $portfile
fi


#find line with correct device
#expected something like [26749.888939] usb 1-1.3: Product: USB-COM422 Plus2
devstr=$(dmesg | grep "USB-COM422")
echo "dev $devstr"


#extract usb number
usbnum=$(echo $devstr | sed -r 's/.*usb ([0-9]-[0-9]\.[0-9]).*/\1/g')
echo $usbnum


#extract correct ttyUSBs
#example line: [26749.892387] usb 1-1.3: FTDI USB Serial Device converter now attached to ttyUSB3
ttyinfostr=$(dmesg | grep "usb $usbnum" | grep ttyUSB | sed -r 's/.*(ttyUSB[0-9]).*/\1/g')


#usually two ttyUSBs are found, extract the first one
stna=$(echo "tostr $ttyinfostr")
rdev="$(awk -F ' ' '{print $2}' <<< "$stna")"
echo "final device: /dev/$rdev"


#run settings and write the correct ttyUSB device to file:
stty -F /dev/$rdev raw
stty -F /dev/$rdev 921600
stty -F /dev/$rdev
#stty -F /dev/$rdev


echo "/dev/$rdev" > /home/tbuser/koodit/sensors/threaded/refport.txt
#echo "port file written"

