#!/bin/sh
make clean
make

#keyboard=/dev/input/by-id/usb-HTLTEK_Gaming_keyboard-event-kbd
keyboard=/dev/input/by-id/usb-Logitech_USB_Receiver-event-kbd
./part3 $keyboard
