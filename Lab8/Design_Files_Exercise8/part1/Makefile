all: part1

part1: part1.c ../physical.c
	gcc -o part1 part1.c ../physical.c -lm 

.PHONY:	all clean

clean:
	rm -f *.o part1
