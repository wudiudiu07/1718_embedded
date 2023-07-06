#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define video_BYTES 9 // number of characters to read from /dev/video

int screen_x, screen_y;

int main(int argc, char *argv[]){
    int video_FD; // file descriptor
    char buffer[video_BYTES]; // buffer for data read from /dev/video
    char command[64]; // buffer for commands written to /dev/video
    int x, y;

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
    write (video_FD, command, strlen(command)+1);

    printf("Coloring: %d %d\n", screen_x, screen_y);

    // Use pixel commands to color some pixels on the screen
    for (y=0; y < screen_y; ++y) {
        for (x=0; x < screen_x; ++x) {
            if (x < screen_y / 3) {
                sprintf(command, "pixel %d,%d F800", x, y); // Red
            }
            else if (x < screen_y - (screen_y / 3)) {
                sprintf(command, "pixel %d,%d 1F", x, y); // Blue
            }
            else {
                sprintf(command, "pixel %d,%d 7E0", x, y); // Green
            }
            write (video_FD, command, strlen(command)+1);
        }
    }

    close (video_FD);
    return 0;
}