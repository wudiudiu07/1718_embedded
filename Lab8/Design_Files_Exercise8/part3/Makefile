all: part3

part3: part3.c ../physical.c
	gcc -o part3 part3.c -lm -pthread ../physical.c

.PHONY: all clean

clean:
	rm -f *.o part3 