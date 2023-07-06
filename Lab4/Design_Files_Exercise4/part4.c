#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <poll.h>

#define BYTES 256				// max number of characters to read from /dev/chardev

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
    stop = 1;
}

/* Game:
first phase a default stopwatch time is shown on the seven-segment displays
user can change the displayed time by using the SW switches and KEYs

Pressing KEY0 starts the game

At this point the program should present a series of math questions that the user needs to answer within the stopwatch time
Answers should be entered through the command line
Incorrect answers to a question should be rejected, but the user should be allowed to try again as long as the time has not expired

After receiving a correct answer, the stopwatch should be reset and a new question asked
you could increase the difficultly of questions over time.

when the user fails to respond within the stopwatch time, some statistics about the results should be shown to the user
(number of questions correctly answered, average time taken per question)
*/

// Returns: time_mm_out,time_ss_out, time_dd_out
int CheckStopwatch(int stopwatch_fd, unsigned int *time_mm_out, unsigned int *time_ss_out, unsigned int *time_dd_out) {
    int ret_val;
    char chardev_buffer[BYTES];	// buffer for chardev character data
    int stop = 0;

    // Check if stopwatch finished
    while((ret_val = read (stopwatch_fd, chardev_buffer, BYTES)) != 0 );
    sscanf(chardev_buffer, "%02d:%02d:%02d\n", time_mm_out, time_ss_out, time_dd_out);
    if (*time_mm_out == 0 && *time_ss_out == 0 && *time_dd_out == 0) {
        stop = 1;
    }

    return stop;
}

void WaitForKey0(int stopwatch_fd, int key_fd, int sw_fd, int ledr_fd, int hex_fd, 
    unsigned int *time_mm_out, unsigned int *time_ss_out, unsigned int *time_dd_out) { 	
    char chardev_buffer[BYTES];	// buffer for chardev character data

	int ret_val;			    // number of characters read from chardev
    unsigned int key_value = 0; // Needs to be unsigned for conversion to work
    unsigned int sw_value = 0;

    // allow user to change the time by using the SW switches and KEYs
    while (!stop) {
        // read key press
        while((ret_val = read (key_fd, chardev_buffer, BYTES)) != 0 );
        sscanf(chardev_buffer, "%01x", &key_value);

        // Pressing KEY0 should start game
        if (key_value & 0x1) {
            // Read start time
            CheckStopwatch(stopwatch_fd, time_mm_out, time_ss_out, time_dd_out);

            sprintf(chardev_buffer, "run");
            ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer) + 1);
            if (ret_val <= 0) {
                printf("Writing run failed\n");
            }
            break; // Exit this loop to start game loop
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
}

void Game(int stopwatch_fd, unsigned int start_time_mm, unsigned int start_time_ss, unsigned int start_time_dd) {
    int arg1, arg2;
    int user_ans;
    int difficulty = 10;
    time_t t;
    char chardev_buffer[BYTES];	// buffer for chardev character data
	int ret_val;			    // number of characters read from chardev

    unsigned int correct_ans = 0;
    float seconds_avg = 0.0;
    unsigned int time_mm, time_ss, time_dd;
    int timeout = 0;

    // Polling on stdin to make sure not waiting longer than stopwatch
    struct pollfd mypoll = { STDIN_FILENO, POLLIN|POLLPRI };

    // Intializes random number generator
    srand((unsigned) time(&t));

    while (!stop && 0 == CheckStopwatch(stopwatch_fd, &time_mm, &time_ss, &time_dd)) {
        arg1 = rand() % difficulty;
        arg2 = rand() % difficulty;
        
        // At this point the program should present a series of math questions that the user needs to answer
        printf("%d + %d = ", arg1, arg2);
        fflush(stdout);
        if( poll(&mypoll, 1, (time_mm * 60000) + (time_ss * 1000) + (time_dd * 10)) ) {
            scanf("%d", &user_ans);
        }
        printf("\n");

        // Incorrect answers should be rejected, but user should be allowed to try again as long as the time has not expired
        while (arg1 + arg2 != user_ans && 0 == (timeout = CheckStopwatch(stopwatch_fd, &time_mm, &time_ss, &time_dd))) {
            printf("Try again: ");
            fflush(stdout);
            if( poll(&mypoll, 1, (time_mm * 60000) + (time_ss * 1000) + (time_dd * 10)) ) {
                scanf("%d", &user_ans);
            }
            printf("\n");
        }

        // Check if out-of-time when leaving loop
        if (timeout) {
            break;
        }

        ++correct_ans;
        seconds_avg += (time_mm * 60) + time_ss + (time_dd / 100);
        seconds_avg /= correct_ans;

        // After receiving a correct answer, stopwatch should be reset and a new question asked
        sprintf(chardev_buffer, "%02d:%02d:%02d", start_time_mm, start_time_ss, start_time_dd);
        ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
        if (ret_val <= 0) {
            printf("Writing Start time failed\n");
        }

        // you could increase the difficultly of questions over time.
        if (correct_ans % 5 == 0 && correct_ans != 0) {
            difficulty *= 10;
        }
    }

    // when the user fails to respond within the stopwatch time, some statistics about the results should be shown to the user
    // (number of questions correctly answered, average time taken per question)
    printf("Time expired! You answered %d questions, in an average of %.2f seconds\n", correct_ans, seconds_avg);
}

int main(int argc, char *argv[]) {
	int stopwatch_fd;
    int key_fd, sw_fd;          // file descriptor - for reading
    int ledr_fd, hex_fd;        // file descriptor - for writing
  	
    char chardev_buffer[BYTES];	// buffer for chardev character data

	int ret_val;			    // number of characters read from chardev
    unsigned int time_mm, time_ss, time_dd; // Initial stopwatch used

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

    if ((hex_fd = open("/dev/HEX", O_RDWR)) == -1)
	{
		printf("Error opening /dev/HEX: %s\n", strerror(errno));
		return -1;
	}

    if ((stopwatch_fd = open("/dev/stopwatch", O_RDWR)) == -1)
	{
		printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
		return -1;
	}

    // Ensure stopwatch is not currently running
    sprintf(chardev_buffer, "stop");
    ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer) + 1);
    if (ret_val <= 0) {
        printf("Writing run/stop failed\n");
    }

    // first phase of game - setup
    // set default stopwatch time on seven-segment displays
    sprintf(chardev_buffer, "00:15:99");
    ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
    if (ret_val <= 0) {
        printf("Writing Start time failed\n");
    }

    // Display Time on HEX
    sprintf(chardev_buffer, "disp");
    ret_val = write (stopwatch_fd, chardev_buffer, strlen(chardev_buffer)+1);
    if (ret_val <= 0) {
        printf("Writing to disp failed\n");
    }

    // Clear KEY press by reading it
    while((ret_val = read (key_fd, chardev_buffer, BYTES)) != 0 )
    printf("Welcome to Game\n");
    printf("Set stopwatch time if desired. Then press KEY0 to begin:\n");

    // starts the game when KEY0 pressed
    WaitForKey0(stopwatch_fd, key_fd, sw_fd, ledr_fd, hex_fd, 
            &time_mm, &time_ss, &time_dd);

    // Run Game
    Game(stopwatch_fd, time_mm, time_ss, time_dd);

    close (stopwatch_fd);
	close (hex_fd);
    close (ledr_fd);
    close (sw_fd);
    close (key_fd);
    
   return 0;
}