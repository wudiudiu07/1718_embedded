#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define BYTES 256				// max number of characters to read from /dev/chardev

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
    stop = 1;
}

int main(int argc, char *argv[]) {
	int stopwatch_fd;
    int key_fd, sw_fd;  // file descriptor - for reading
    int ledr_fd;        // file descriptor - for writing
  	
    char chardev_buffer[BYTES];	// buffer for chardev character data

	int ret_val;			    // number of characters read from chardev
    unsigned int key_value = 0; // Needs to be unsigned for conversion to work
    unsigned int sw_value = 0;
    int key0_press = 0;    // bool toggle

  	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);
    
    // open the files /dev/KEY and /dev/SW for reading
    // open the files /dev/LEDR and /dev/HEX for writing

	// Open the character device driver for read
	if ((key_fd = open("/dev/KEY", O_RDWR)) == -1)
	{
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
		return -1;
	}

    if ((sw_fd = open("/dev/SW", O_RDWR)) == -1)
	{
		printf("Error opening /dev/SW: %s\n", strerror(errno));
		return -1;
	}
    
    // Open the character device driver for write
	if ((ledr_fd = open("/dev/LEDR", O_RDWR)) == -1)
	{
		printf("Error opening /dev/LEDR: %s\n", strerror(errno));
		return -1;
	}

    if ((stopwatch_fd = open("/dev/stopwatch", O_RDWR)) == -1)
	{
		printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
		return -1;
	}

    // Reset Time
    sprintf(chardev_buffer, "59:59:99");
    ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
    if (ret_val <= 0) {
        printf("Writing Start time failed\n");
    }

    // Display HEX
    sprintf(chardev_buffer, "disp");
    ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
    if (ret_val <= 0) {
        printf("Writing to disp failed\n");
    }

    // Run
    sprintf(chardev_buffer, "run");
    ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
    if (ret_val <= 0) {
        printf("Writing to run failed\n");
    }

    while (!stop) {
        // read key press
        while((ret_val = read (key_fd, chardev_buffer, BYTES)) != 0 );
        sscanf(chardev_buffer, "%01x", &key_value);
        //printf("Key: %s == %d\n", chardev_buffer, key_value);    

        // Pressing KEY0 should toggle between the run and pause states
        if (key_value & 0x1) {
            key0_press = (key0_press == 0) ? 1 : 0; // flip on key0 press
            sprintf(chardev_buffer, (key0_press == 0) ? "run" : "stop");
            ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer) + 1);
            if (ret_val <= 0) {
                printf("Writing run/stop failed\n");
            }
        }

        // set the time based on the values of the switches SW
        // Read SW
        while((ret_val = read (sw_fd, chardev_buffer, BYTES)) != 0 );
        //printf("SW: %s\n", chardev_buffer);          
        sscanf(chardev_buffer, "%03x", &sw_value);

        // may also want to display SW values on the LED switches
        // Write SW to LEDR
        //printf("LEDR: %s\n", chardev_buffer);
		ret_val = write (ledr_fd, chardev_buffer, strlen(chardev_buffer)+1);
        if (ret_val <= 0) {
            printf("Issue writing to /dev/LEDR\n");
        }

        // KEY1 => When pressed use the values of the SW switches to set the DD part of the stopwatch time. maximum value is 99
        if (key_value & 0b0010) {
            sw_value = (sw_value > 99) ? 99 : sw_value;
            sprintf(chardev_buffer, "::%02d", sw_value);
            printf("%s\n", chardev_buffer);
            ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
            if (ret_val <= 0) {
                printf("Issue writing dd\n");
            }
        }

        // KEY2 => When pressed, use the values of the SW switches to set the SS part of the stopwatch time. maximum value is 59
        if (key_value & 0b0100) {
            sw_value = (sw_value > 59) ? 59 : sw_value;
            sprintf(chardev_buffer, ":%02d:", sw_value);
            printf("%s\n", chardev_buffer);
            ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
            if (ret_val <= 0) {
                printf("Issue writing ss\n");
            }
        }

        // KEY3 => When pressed, use the values of the SW switches to set the MM part of the stopwatch time. maximum value is 59
        if (key_value & 0b1000) {
            sw_value = (sw_value > 59) ? 59 : sw_value;
            sprintf(chardev_buffer, "%02d::", sw_value);
            printf("%s\n", chardev_buffer);
            ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
            if (ret_val <= 0) {
                printf("Issue writing mm\n");
            }
        }
    }

    close (stopwatch_fd);
    close (ledr_fd);
    close (sw_fd);
    close (key_fd);
    
   return 0;
}