all: part1

part1: edgedetect.c
	gcc -g -Wall -o part1 edgedetect.c -lintelfpgaup -lm

.PHONY:	clean

clean:
	rm -f *.o part1 *.bmp
