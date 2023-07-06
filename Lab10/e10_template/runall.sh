#!/bin/bash


if [ $# -lt 1 ]
then
        echo "Usage : ./runall.sh 1|2|3 <absolute path to image file>"
        echo "        ./runall.sh clean"
        echo "        ./runall.sh submit"
        exit
fi

if [ "$1" = "clean" ]
then
   echo "-- Cleaning Up! --"
   rmmod -f video
   cd part1 && make clean && cd ..
   cd part2 && make clean && cd ..
   cd part3 && make clean && cd ..
   /home/root/misc/program_fpga ./DE1_SoC_Computer.rbf
   exit
fi

if [ "$1" = "submit" ]
then
   echo "--- Creating submission archive.  Please upload to Quercus! ---"
   rmmod -f video
   /home/root/misc/program_fpga ./DE1_SoC_Computer.rbf
   cd part1 && make clean && cd ..
   cd part2 && make clean && cd ..
   cd part3 && make clean && cd ..
   mkdir -p submissions
   tar -cjvf submissions/e10_$(date "+%Y.%m.%d-%H.%M.%S").tar.bz2 images include part1 part2 part3 README.txt runall.sh DE1_SoC_Computer.rbf Edge_Detector_System.rbf
   echo "---   Created submission archive.  Please upload to Quercus!   ---"
   exit
fi

if [[ $# -lt 2 ]]
then
        echo "Usage : ./runall.sh 1|2|3 <absolute path to image file>"
        exit
fi

IMG=$2
if [ "$IMG" == "${IMG#/}" ]
then
  echo "Usage : must use absolute path to image file"
  exit
fi

case "$1" in

1)  echo "-- Setting up Part 1 --"
    rmmod -f video
    /home/root/misc/program_fpga ./DE1_SoC_Computer.rbf
    insmod /home/root/Linux_Libraries/drivers/video.ko
    cd part1 && make clean && make
    echo "-- Running Part 1 --"
    echo "-- Output files appear in ./part1/ --"
    eval "./part1 -v -d $2"
    ;;

2)  echo "-- Setting up Part 2 --"
    rmmod -f video
    /home/root/misc/program_fpga ./Edge_Detector_System.rbf
    cd part2 && make clean && make
    echo "-- Running Part 2 --"
    eval "./part2 $2"
    ;;

3)  echo "-- Setting up Part 3 --"
    rmmod -f video
    /home/root/misc/program_fpga ./Edge_Detector_System.rbf
    cd part3 && make clean && make
    echo "-- Running Part 3 --"
    eval "./part3 $2"
    ;;

*) echo "Usage : ./runall.sh 1|2|3 <absolute path to image file>"
   echo "        ./runall.sh clean"
   echo "        ./runall.sh submit"
   ;;

esac
