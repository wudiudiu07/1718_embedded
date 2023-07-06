all: part1

part1: part1.c ../ADXL345.c ../physical.c
	gcc -g -Wall -o part1 part1.c ../ADXL345.c ../physical.c

.PHONY:	all clean

clean:
	rm -f *.o part1