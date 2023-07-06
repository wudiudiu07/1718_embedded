#!/bin/sh
make clean
make

rmmod video
rmmod audio
rmmod KEY
rmmod LEDR
rmmod stopwatch

insmod /home/root/Linux_Libraries/drivers/video.ko
insmod /home/root/Linux_Libraries/drivers/audio.ko
insmod ../stopwatch.ko
insmod /home/root/Linux_Libraries/drivers/KEY.ko
insmod /home/root/Linux_Libraries/drivers/LEDR.ko

rm -f /dev/video
rm -f /dev/KEY
rm -f /dev/LEDR

ln -s /dev/IntelFPGAUP/video /dev/video
ln -s /dev/IntelFPGAUP/KEY /dev/KEY
ln -s /dev/IntelFPGAUP/LEDR /dev/LEDR

#keyboard=/dev/input/by-id/usb-HTLTEK_Gaming_keyboard-event-kbd
keyboard=/dev/input/by-id/usb-Logitech_USB_Receiver-event-kbd
./part6 $keyboard