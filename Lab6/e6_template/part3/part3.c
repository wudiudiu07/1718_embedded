#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#define video_BYTES 9 // number of characters to read from /dev/video

int screen_x, screen_y;

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
    stop = 1;
}

int strLenBytes(char *str) {
    return strlen(str) + 1;
}

int main(int argc, char *argv[]){
    int video_FD; // file descriptor
    char buffer[video_BYTES]; // buffer for video char data
    char command[64]; // buffer for commands written to /dev/video
    int y;
    int direction=-1;

  	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1)
    {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    // Get screen_x and screen_y by reading from the driver
    while(read (video_FD, buffer, video_BYTES) != 0);
    sscanf(buffer, "%d %d", &screen_x, &screen_y);

    // Clear Screen
    sprintf(command, "clear"); // Green
    write (video_FD, command, strLenBytes(command));

    printf("Bouncing Lining: %d %d\n", screen_x, screen_y);
   
    y = 0;
    while (!stop) {
        // commands have new line at end to match 
        sprintf (command, "line %d,%d %d,%d %x", 0, y, screen_x - 1, y, 0xF800); // red
        write (video_FD, command, strLenBytes(command));

        sprintf (command, "sync");
        write (video_FD, command, strLenBytes(command));

        sprintf (command, "line %d,%d %d,%d %x", 0, y, screen_x - 1, y, 0); // Clear
        write (video_FD, command, strLenBytes(command));

        // Update
        if (y == (screen_y-1) || y == 0) {
            direction = (direction > 0) ? -1 : 1;
        }
        y += direction;
    }

    close (video_FD);
    return 0;
}