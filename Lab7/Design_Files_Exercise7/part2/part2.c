#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "../address_map_arm.h"

volatile sig_atomic_t stop = 0;
void catchSIGINT(int signum){
    stop = 1;
}

int main(void) 
{
    int accel_fd = -1;    // used to open /dev/mem for access to physical addresses
    char chardev_buffer[256]; // buffer for reading from drivers

  	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);

    // Open the character device driver
    if ((accel_fd = open("/dev/accel", O_RDWR)) == -1)
    {
        printf("Error opening /dev/accel: %s\n", strerror(errno));
        return -1;
    }

    while (!stop) {
        while(read (accel_fd, chardev_buffer, sizeof(chardev_buffer)) != 0 )
        if (chardev_buffer[0] == '1') { 
            printf("%s", chardev_buffer);
        }
    }

    close(accel_fd);

    return 0;
}
