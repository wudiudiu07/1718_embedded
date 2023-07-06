all: part2

part2: edgedetect.c physical.c
	gcc -I../include -Wall -o part2 edgedetect.c physical.c

.PHONY:	clean

clean:
	rm -f *.o *.bmp part2
