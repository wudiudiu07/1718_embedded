all: part3

part3: edgedetect.c physical.c
	gcc -I../include -Wall -o part3 edgedetect.c physical.c

.PHONY:	clean

clean:
	rm -f *.o *.bmp part3
