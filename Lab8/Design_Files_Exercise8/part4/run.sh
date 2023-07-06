#!/bin/sh
make clean
make

rmmod video
insmod /home/root/Linux_Libraries/drivers/video.ko

#keyboard=/dev/input/by-id/usb-HTLTEK_Gaming_keyboard-event-kbd
keyboard=/dev/input/by-id/usb-Logitech_USB_Receiver-event-kbd
./part4 $keyboard