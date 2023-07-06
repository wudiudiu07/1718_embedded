obj-m += accel.o
accel-objs := ../ADXL345.o accel_main.o

all: accel part2

accel:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules KCPPFLAGS="-DTHIS_IS_DRIVER=1"

part2: part2.c
	gcc -g -Wall -o part2 part2.c

.PHONY:	all clean

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *.o part2
	rm -f ../.ADXL345.o.cmd ../*.o