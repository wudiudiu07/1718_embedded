all: part2

part2: part2.c ../physical.c
	gcc -o part2 part2.c ../physical.c -lm 

.PHONY:	all clean

clean:
	rm -f *.o part2
