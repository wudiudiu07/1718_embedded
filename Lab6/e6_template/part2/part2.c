#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define video_BYTES 9 // number of characters to read from /dev/video

int screen_x, screen_y;

int main(int argc, char *argv[]){
    int video_FD; // file descriptor
    char buffer[video_BYTES]; // buffer for video char data
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

    printf("Lining: %d %d\n", screen_x, screen_y);

    /* Draw a few lines */
    sprintf (command, "line %d,%d %d,%d %x", 0, screen_y - 1, screen_x - 1, screen_y - 20, 0x551F);
    write (video_FD, command, strlen(command)+1);

    sprintf (command, "line %d,%d %d,%d %x", 0, screen_y - 1, screen_x - 1, 0, 0xFFE0); // yellow
    write (video_FD, command, strlen(command)+1);

    sprintf (command, "line %d,%d %d,%d %x", 0, screen_y - 1, (screen_x >> 1) - 1, 0, 0x07FF); // cyan
    write (video_FD, command, strlen(command)+1);

    sprintf (command, "line %d,%d %d,%d %x", 0, screen_y - 1, (screen_x >> 2) - 1, 0, 0x07E0); // green
    write (video_FD, command, strlen(command)+1);

    sprintf (command, "line %d,%d %d,%d %x", 0, screen_y - 1, (screen_x >> 3) - 1, 0, 0xF800); // red
    write (video_FD, command, strlen(command)+1);

    close (video_FD);
    return 0;
}