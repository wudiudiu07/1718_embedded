#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "../address_map_arm.h"

#define X_MIN 1
#define X_MAX 80
#define Y_MIN 1
#define Y_MAX 24

#define RED 31
#define GREEN 32
#define YELLOW 33
#define BLUE 34
#define MAGENTA 35
#define CYAN 36
#define WHITE 37

static char colors[] = {WHITE, RED, YELLOW, GREEN, CYAN, MAGENTA, BLUE};

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
    stop = 1;
}

void plot_pixel(int x, int y, char color, char c)
{
  	printf ("\e[%2dm\e[%d;%dH%c", color, y, x, c);
  	fflush (stdout);
}

void draw_bubble(int x, int y, char color) {
    plot_pixel(x, y, color, 'O');
}

int main(void) 
{
    int accel_fd = -1;    // used to open /dev/mem for access to physical addresses
    char chardev_buffer[256]; // buffer for reading from drivers
    int newData, X, Y, Z, mg_per_lsb;
    unsigned int x, y;
    int alpha;
    int av_X, av_Y, av_Z; // av_x is the average acceleration

    alpha = 0.1; //effect of new data on the average

  	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);

    // Open the character device driver
    if ((accel_fd = open("/dev/accel", O_RDWR)) == -1)
    {
        printf("Error opening /dev/accel: %s\n", strerror(errno));
        return -1;
    }

    printf ("\e[2J"); 					// clear the screen
  	printf ("\e[?25l");					// hide the cursor

    //first position is in he middle of the screen
    x = X_MAX/2; y = Y_MAX/2; 
    av_X = 0; av_Y = 0;
    draw_bubble(x, y, GREEN);

    while (!stop) {
        while(read (accel_fd, chardev_buffer, sizeof(chardev_buffer)) != 0 )
        if (chardev_buffer[0] == '1') { 
            sscanf(chardev_buffer, "%d %d %d %d %d", &newData, &X, &Y, &Z, &mg_per_lsb);

            av_X = av_X * alpha + X * (1 - alpha);
            av_Y = av_Y * alpha + Y * (1 - alpha);

            // Check if tilt right
            if (av_X > 10 && x < X_MAX) {
                x += 1;
            }
            // Check if tile left
            else if (av_X < -10 && 0 < x) {
                x -= 1;
            }

            // Check if tilt towards me -> then move bubble down
            if (av_Y < -10 && y < Y_MAX) {
                y += 1;
            }
            else if (av_Y > 10 && 0 < y) {
                y -= 1; 
            }            

            // Draw
            printf ("\e[2J"); // clear the screen
            printf ("\e[%2dm\e[%d;%dHX=%d Y=%d Z=%d", WHITE, 0, 0, X, Y, Z);
            printf(" Adding %d,%d avgX=%d avgY=%d\n", x, y, av_X, av_Y);
            draw_bubble(x, y, GREEN);
        }
    }

	printf ("\e[2J"); 			// clear the screen
	printf ("\e[%2dm", WHITE);	// reset foreground color
	printf ("\e[%d;%dH", 1, 1); // move cursor to upper left
	printf ("\e[?25h");			// show the cursor
	fflush (stdout);

    close(accel_fd);

    return 0;
}