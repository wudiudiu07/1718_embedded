#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include "address_map_arm.h"
#include <string.h>

// used to exit the program cleanly
volatile sig_atomic_t stop;
void catchSIGINT(int);

void outTerminal(char *msg, int offset) {
    const int displaySize = 6;
    char display[displaySize + 1];
    int i = 0;
    
    for (i = 0; i < displaySize; ++i, ++offset) {
        if (msg[offset] == '\0') {
            offset = 0;
        }
        display[i] = msg[offset];
    }
    display[6] = '\0';

    // Clear terminal window
    printf ("\e[2J");
    fflush (stdout);

    // Draw Box for 6 digits
    printf("\e[01;100H ------ \n");
    printf("\e[02;100H|\e[31m%s\e[00m|\n", display);
    printf("\e[03;100H ------ \n");
}

int main(void)
{      
    struct timespec ts;
	time_t start_time, elapsed_time;

    // Setup scroll loop delay
    ts.tv_sec = 0;										// used to delay
	ts.tv_nsec = 500000000;							// 10^8 ns = 0.1 sec

	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
    signal(SIGINT, catchSIGINT);
    start_time = time (NULL);
    
    // Scroll Message on HEX LEDS
    char msg[] = "Intel SoC FPGA ";

    int epoch = 0;
    while (!stop) {
        outTerminal(msg, epoch);
            
        /* wait for timer */
        nanosleep (&ts, NULL);
        elapsed_time = time (NULL) - start_time;
        if (elapsed_time > 1000 || elapsed_time < 0) stop = 1;
        
        ++epoch;
        if (epoch > strlen(msg)) {
            epoch = 0;
        }
    }
    
   return 0;
}


/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
	stop = 1;
}